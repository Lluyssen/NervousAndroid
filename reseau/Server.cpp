#include "Protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

using namespace ng;
using namespace ng::net;

namespace {

struct Client {
    int fd = -1;
    std::string inbuf;
    bool connected() const { return fd >= 0; }
};

void sendLine(Client& c, const std::string& line) {
    if (!c.connected()) return;
    std::string s = line + "\n";
    size_t off = 0;
    while (off < s.size()) {
        ssize_t n = ::send(c.fd, s.data() + off, s.size() - off, MSG_NOSIGNAL);
        if (n <= 0) { ::close(c.fd); c.fd = -1; return; }
        off += static_cast<size_t>(n);
    }
}

void closeClient(Client& c) {
    if (c.connected()) ::close(c.fd);
    c.fd = -1;
    c.inbuf.clear();
}

int makeListener(int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        ::listen(fd, 2) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

}  // namespace

int main(int argc, char** argv) {
    int port = argc > 1 ? std::atoi(argv[1]) : 5000;
    int w = argc > 2 ? std::atoi(argv[2]) : 20;
    int h = argc > 3 ? std::atoi(argv[3]) : 20;

    ::signal(SIGPIPE, SIG_IGN);

    std::unique_ptr<Game> game;
    try {
        game = std::make_unique<Game>(w, h);
    } catch (const std::exception& e) {
        std::cerr << "Grille invalide : " << e.what() << "\n";
        return 1;
    }

    int listener = makeListener(port);
    if (listener < 0) { std::perror("listen"); return 1; }
    std::cout << "Serveur sur le port " << port << ", grille " << w << "x" << h << "\n";

    Client clients[2];
    bool started = false;

    auto resetGame = [&] {
        game = std::make_unique<Game>(w, h);
        started = false;
    };

    // Traite une ligne recue du joueur 'idx' (0 ou 1).
    auto handleLine = [&](int idx, const std::string& line) {
        Player who = idx == 0 ? Player::One : Player::Two;
        auto act = decodeAction(line);
        if (!act) {
            ServerMsg rej; rej.type = MsgType::Reject; rej.reason = Result::WrongPhase;
            sendLine(clients[idx], encode(rej));
            return;
        }
        if (!started || game->isOver() || game->actingPlayer() != who) {
            ServerMsg rej; rej.type = MsgType::Reject; rej.reason = Result::WrongPhase;
            sendLine(clients[idx], encode(rej));
            return;
        }

        Result r = apply(*game, *act);
        if (r != Result::Ok) {
            ServerMsg rej; rej.type = MsgType::Reject; rej.reason = r;
            sendLine(clients[idx], encode(rej));
            return;
        }

        ServerMsg out;
        out.type = MsgType::ActionMsg;
        out.actor = who;
        out.action = *act;
        std::string wire = encode(out);
        sendLine(clients[0], wire);
        sendLine(clients[1], wire);
    };

    for (;;) {
        pollfd fds[3];
        fds[0] = {listener, POLLIN, 0};
        fds[1] = {clients[0].fd, POLLIN, 0};   // fd = -1 est ignore par poll
        fds[2] = {clients[1].fd, POLLIN, 0};

        if (::poll(fds, 3, -1) < 0) { std::perror("poll"); break; }

        // Nouvelle connexion
        if (fds[0].revents & POLLIN) {
            int cfd = ::accept(listener, nullptr, nullptr);
            if (cfd >= 0) {
                int slot = !clients[0].connected() ? 0 : (!clients[1].connected() ? 1 : -1);
                if (slot < 0) {
                    ::close(cfd);   // partie pleine
                } else {
                    clients[slot].fd = cfd;
                    clients[slot].inbuf.clear();

                    ServerMsg wm;
                    wm.type = MsgType::Welcome;
                    wm.you = slot == 0 ? Player::One : Player::Two;
                    wm.width = w; wm.height = h;
                    sendLine(clients[slot], encode(wm));
                    std::cout << "Joueur " << (slot + 1) << " connecte\n";

                    if (clients[0].connected() && clients[1].connected() && !started) {
                        started = true;
                        ServerMsg st; st.type = MsgType::Start;
                        sendLine(clients[0], encode(st));
                        sendLine(clients[1], encode(st));
                        std::cout << "Partie lancee\n";
                    }
                }
            }
        }

        // Donnees des clients
        for (int i = 0; i < 2; ++i) {
            if (!clients[i].connected()) continue;
            if (!(fds[i + 1].revents & (POLLIN | POLLHUP | POLLERR))) continue;

            char buf[512];
            ssize_t n = ::recv(clients[i].fd, buf, sizeof(buf), 0);
            if (n <= 0) {
                std::cout << "Joueur " << (i + 1) << " deconnecte\n";
                closeClient(clients[i]);
                int other = 1 - i;
                if (clients[other].connected()) {
                    ServerMsg left; left.type = MsgType::OpponentLeft;
                    sendLine(clients[other], encode(left));
                }
                resetGame();   // la partie en cours est abandonnee
                continue;
            }

            clients[i].inbuf.append(buf, static_cast<size_t>(n));
            if (clients[i].inbuf.size() > 4096) { closeClient(clients[i]); continue; }

            size_t pos;
            while (clients[i].connected() &&
                   (pos = clients[i].inbuf.find('\n')) != std::string::npos) {
                std::string line = clients[i].inbuf.substr(0, pos);
                clients[i].inbuf.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                handleLine(i, line);
            }
        }
    }

    ::close(listener);
    return 0;
}