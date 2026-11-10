#include "NetClient.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

namespace ng::net {

NetClient::~NetClient() { disconnect(); }

bool NetClient::connectTo(const std::string& host, int port) {
    disconnect();
    disconnected_ = false;
    queue_.clear();

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0)
        return false;

    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { ::freeaddrinfo(res); return false; }

    if (::connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        ::close(fd);
        ::freeaddrinfo(res);
        return false;
    }
    ::freeaddrinfo(res);

    // Non bloquant apres la connexion
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    fd_ = fd;
    inbuf_.clear();
    return true;
}

void NetClient::disconnect() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    inbuf_.clear();
}

void NetClient::pushDisconnected() {
    if (disconnected_) return;           // une seule fois
    disconnected_ = true;
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
    inbuf_.clear();

    ServerMsg m;
    m.type = MsgType::Disconnected;
    queue_.push_back(std::move(m));
}

std::optional<ServerMsg> NetClient::poll() {
    // 1. Lire ce qui est disponible sur le socket (si encore ouvert)
    if (fd_ >= 0) {
        char buf[512];
        for (;;) {
            ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
            if (n > 0) {
                inbuf_.append(buf, static_cast<size_t>(n));
                if (inbuf_.size() > 8192) {       // serveur qui deborde : on coupe
                    pushDisconnected();
                    break;
                }
            } else if (n == 0) {                  // fermeture propre par le serveur
                pushDisconnected();
                break;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                pushDisconnected();               // vraie erreur reseau
                break;
            }
        }
    }

    // 2. Decoder toutes les lignes completes vers la file
    //    (on le fait meme apres une deconnexion : les derniers messages
    //     recus avant la coupure doivent etre livres avant "Disconnected")
    size_t pos;
    while ((pos = inbuf_.find('\n')) != std::string::npos) {
        std::string line = inbuf_.substr(0, pos);
        inbuf_.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (auto m = decodeServerMsg(line))       // ligne invalide : ignoree
            queue_.push_back(std::move(*m));
    }

    // 3. Livrer un message de la file
    if (queue_.empty()) return std::nullopt;
    ServerMsg m = std::move(queue_.front());
    queue_.pop_front();
    return m;
}

bool NetClient::send(const Action& a) {
    if (fd_ < 0) return false;
    std::string s = encode(a) + "\n";
    size_t off = 0;
    while (off < s.size()) {
        ssize_t n = ::send(fd_, s.data() + off, s.size() - off, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue; // buffer plein
        if (n <= 0) { pushDisconnected(); return false; }
        off += static_cast<size_t>(n);
    }
    return true;
}

}  // namespace ng::net