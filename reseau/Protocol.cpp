#include "Protocol.h"
#include <sstream>

namespace ng::net {

namespace {

const char* kindName(ActionKind k) {
    switch (k) {
        case ActionKind::Base:     return "BASE";
        case ActionKind::Square:   return "SQUARE";
        case ActionKind::End:      return "END";
        case ActionKind::Relocate: return "RELOCATE";
    }
    return "END";
}

std::optional<ActionKind> parseKind(const std::string& s) {
    if (s == "BASE")     return ActionKind::Base;
    if (s == "SQUARE")   return ActionKind::Square;
    if (s == "END")      return ActionKind::End;
    if (s == "RELOCATE") return ActionKind::Relocate;
    return std::nullopt;
}

bool needsPos(ActionKind k) { return k != ActionKind::End; }

int playerNum(Player p) { return p == Player::One ? 1 : 2; }

std::optional<Player> parsePlayer(int n) {
    if (n == 1) return Player::One;
    if (n == 2) return Player::Two;
    return std::nullopt;
}

// Lit les champs apres le mot-cle : action sans ou avec position.
std::optional<Action> readAction(std::istringstream& in, ActionKind k) {
    Action a;
    a.kind = k;
    if (needsPos(k)) {
        if (!(in >> a.pos.x >> a.pos.y)) return std::nullopt;
    }
    std::string extra;
    if (in >> extra) return std::nullopt;   // trailing garbage
    return a;
}

}  // namespace

// ---------- client -> serveur ----------

std::string encode(const Action& a) {
    std::ostringstream o;
    o << kindName(a.kind);
    if (needsPos(a.kind)) o << ' ' << a.pos.x << ' ' << a.pos.y;
    return o.str();
}

std::optional<Action> decodeAction(const std::string& line) {
    std::istringstream in(line);
    std::string word;
    if (!(in >> word)) return std::nullopt;
    auto k = parseKind(word);
    if (!k) return std::nullopt;
    return readAction(in, *k);
}

// ---------- serveur -> client ----------

std::string encode(const ServerMsg& m) {
    std::ostringstream o;
    switch (m.type) {
        case MsgType::Welcome:
            o << "WELCOME " << playerNum(m.you) << ' ' << m.width << ' ' << m.height;
            break;
        case MsgType::Start:
            o << "START";
            break;
        case MsgType::ActionMsg:
            o << "ACTION " << playerNum(m.actor) << ' ' << encode(m.action);
            break;
        case MsgType::Reject:
            o << "REJECT " << static_cast<int>(m.reason);
            break;
        case MsgType::OpponentLeft:
            o << "OPPONENT_LEFT";
            break;
        case MsgType::Disconnected:
            break;
        case MsgType::OpponentBack:
            o << "OPPONENT_BACK";
            break;
    }
    return o.str();
}

std::optional<ServerMsg> decodeServerMsg(const std::string& line) {
    std::istringstream in(line);
    std::string word;
    if (!(in >> word)) return std::nullopt;

    ServerMsg m;
    if (word == "WELCOME") {
    int n;
    if (!(in >> n >> m.width >> m.height)) return std::nullopt;
    int resumed = 0;
    in >> resumed;                       // optionnel : absent = 0
    auto p = parsePlayer(n);
    if (!p) return std::nullopt;
    m.type = MsgType::Welcome;
    m.you = *p;
    m.resumed = (resumed != 0);
    return m;
}
if (word == "OPPONENT_BACK") {
    m.type = MsgType::OpponentBack;
    return m;
}
    if (word == "START") {
        m.type = MsgType::Start;
        return m;
    }
    if (word == "ACTION") {
        int n;
        std::string kw;
        if (!(in >> n >> kw)) return std::nullopt;
        auto p = parsePlayer(n);
        auto k = parseKind(kw);
        if (!p || !k) return std::nullopt;
        auto a = readAction(in, *k);
        if (!a) return std::nullopt;
        m.type = MsgType::ActionMsg;
        m.actor = *p;
        m.action = *a;
        return m;
    }
    if (word == "REJECT") {
        int code;
        if (!(in >> code)) return std::nullopt;
        m.type = MsgType::Reject;
        m.reason = static_cast<Result>(code);
        return m;
    }
    if (word == "OPPONENT_LEFT") {
        m.type = MsgType::OpponentLeft;
        return m;
    }
    return std::nullopt;
}

// ---------- application sur un Game ----------

Result apply(Game& g, const Action& a) {
    switch (a.kind) {
        case ActionKind::Base:     return g.placeBase(a.pos);
        case ActionKind::Square:   return g.placeSquare(a.pos);
        case ActionKind::End:      return g.endTurn();
        case ActionKind::Relocate: return g.relocateBase(a.pos);
    }
    return Result::WrongPhase;
}

}  // namespace ng::net