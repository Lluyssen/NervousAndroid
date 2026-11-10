#pragma once

#include <deque>
#include <optional>
#include <string>

#include "Protocol.h"

namespace ng::net {

class NetClient {
public:
    NetClient() = default;
    ~NetClient();

    NetClient(const NetClient&) = delete;
    NetClient& operator=(const NetClient&) = delete;

    bool connectTo(const std::string& host, int port);
    void disconnect();

    // Retourne le prochain message du serveur, ou nullopt s'il n'y en a pas.
    // Emet un message MsgType::Disconnected (une seule fois) si la connexion tombe.
    std::optional<ServerMsg> poll();

    bool send(const Action& a);

    bool isConnected() const { return fd_ >= 0 && !disconnected_; }

private:
    void pushDisconnected();

    int fd_ = -1;
    bool disconnected_ = false;
    std::string inbuf_;
    std::deque<ServerMsg> queue_;
};

}  // namespace ng::net