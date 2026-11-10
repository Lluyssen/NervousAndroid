#include "NetClient.h"
#include <chrono>
#include <iostream>
#include <thread>

using namespace ng::net;

int main() {
    NetClient c;
    if (!c.connectTo("127.0.0.1", 5000)) { std::cerr << "connexion impossible\n"; return 1; }
    std::cout << "connecte\n";

    for (int i = 0; i < 200 && c.isConnected(); ++i) {
        while (auto m = c.poll()) {
            switch (m->type) {
                case MsgType::Welcome:      std::cout << "WELCOME joueur " << static_cast<int>(m->you) << "\n"; break;
                case MsgType::Start:        std::cout << "START\n"; break;
                case MsgType::ActionMsg:    std::cout << "ACTION recue\n"; break;
                case MsgType::Reject:       std::cout << "REJECT\n"; break;
                case MsgType::OpponentLeft: std::cout << "OPPONENT_LEFT\n"; break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}