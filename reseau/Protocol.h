#pragma once
#include <optional>
#include <string>
#include "Game.h"   // Pos, Player, Result

namespace ng::net {

enum class ActionKind { Base, Square, End, Relocate };

struct Action {
    ActionKind kind = ActionKind::End;
    Pos pos{0, 0};            // ignore pour End
};

enum class MsgType { Welcome, Start, ActionMsg, Reject, OpponentLeft, Disconnected };

struct ServerMsg {
    MsgType type = MsgType::Start;
    // Welcome
    Player you = Player::One;
    int width = 0, height = 0;
    // ActionMsg
    Player actor = Player::One;
    Action action;
    // Reject
    Result reason = Result::Ok;
};

// Client -> serveur
std::string encode(const Action& a);
std::optional<Action> decodeAction(const std::string& line);

// Serveur -> client
std::string encode(const ServerMsg& m);
std::optional<ServerMsg> decodeServerMsg(const std::string& line);

// Applique une action a un Game (utilise par le serveur ET par les clients).
Result apply(Game& g, const Action& a);

}  // namespace ng::net