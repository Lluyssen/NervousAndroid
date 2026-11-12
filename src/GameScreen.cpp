#include "GameScreen_Internal.h"
#include <fstream>

namespace ng {

GameScreen::GameScreen(int width, int height) : game_(width, height) {}

GameScreen::GameScreen(Game loaded, GameMode mode)
    : game_(std::move(loaded)), resumed_(true) {
    setMode(mode);
}

GameScreen::~GameScreen() {
    if (aiThread_.joinable()) aiThread_.join();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_)   SDL_DestroyWindow(window_);
}

void GameScreen::tryReconnect() {
    const Uint64 now = SDL_GetTicks();
    if (now < nextRetryTicks_) return;
    nextRetryTicks_ = now + std::min<Uint64>(5000, 1000 * (retryCount_ + 1));
    ++retryCount_;

    auto fresh = std::make_unique<net::NetClient>();
    if (!fresh->connectTo(host_, port_)) {
        setMessage("Reconnexion... (essai " + std::to_string(retryCount_) + ")");
        return;
    }

    // On repart d'une partie vierge : le serveur va rejouer l'historique.
    const Board& b = game_.board();
    game_ = Game(b.width(), b.height());
    syncHp();
    rebuildCache();

    net_ = std::move(fresh);
    connectionLost_ = false;
    gameStarted_    = false;
    opponentLeft_   = false;
    replaying_      = true;
    retryCount_     = 0;
    setMessage("Reconnecte, resynchronisation...");
}

bool GameScreen::connectOnline(const std::string& host, int port) {
    host_ = host; port_ = port;
    net_ = std::make_unique<net::NetClient>();
    if (!net_->connectTo(host, port)) { net_.reset(); return false; }

    // Le mode (Host ou Join) a deja ete fixe par setMode() / le constructeur.
    // On ne l'ecrase plus : online() se base sur la presence de net_.

    // Attend le WELCOME (max ~5 s) pour connaitre joueur + taille de grille.
    const Uint64 t0 = SDL_GetTicks();
    while (SDL_GetTicks() - t0 < 5000) {
        if (auto m = net_->poll()) {
            if (m->type == net::MsgType::Welcome) {
                myPlayer_ = m->you;
                // Une partie chargee garde sa grille ; sinon on prend celle du serveur.
                if (!resumed_) game_ = Game(m->width, m->height);
                return true;
            }
        }
        SDL_Delay(10);
    }
    net_.reset();
    return false;
}

bool GameScreen::canAct() const {
    if (paused_ || game_.isOver() || isComputerTurn()) return false;
    if (online())
        return gameStarted_ && !opponentLeft_ && !connectionLost_ && !replaying_
               && currentActor() == myPlayer_;
    return true;
}

void GameScreen::pollNetwork() {
    if (!net_) return;
    while (auto m = net_->poll()) {
        switch (m->type) {
            case net::MsgType::Welcome:
                if (replaying_) {
                    // Reprise : on part d'un etat vierge, les ACTION suivent.
                    myPlayer_ = m->you;
                    game_ = Game(m->width, m->height);
                    syncHp();
                    rebuildCache();
                }
                break;   // sinon deja traite dans connectOnline
            case net::MsgType::Start: {
                const bool wasReplay = replaying_;
                gameStarted_ = true;
                replaying_   = false;
                syncHp();
                if (wasReplay)
                    setMessage("Partie reprise.");
                else
                    setMessage(myPlayer_ == Player::One
                                   ? "A vous : placez votre base."
                                   : "L'adversaire place sa base.");
                rebuildCache();
                break;
            }
            case net::MsgType::ActionMsg:
                afterStateChange(net::apply(game_, m->action));
                if (replaying_) syncHp();   // evite un faux flash de PV perdu
                break;
            case net::MsgType::Reject:
                setMessage(describe(m->reason));
                break;
            case net::MsgType::OpponentLeft:
                if (!game_.isOver()) {
                    opponentLeft_ = true;
                    setMessage("L'adversaire a quitte, attente de son retour...");
                }
                break;
            case net::MsgType::OpponentBack:
                opponentLeft_ = false;
                setMessage("L'adversaire est revenu.");
                break;
            case net::MsgType::Disconnected:
                connectionLost_ = true;
                gameStarted_    = false;
                replaying_      = false;
                nextRetryTicks_ = 0;
                retryCount_     = 0;
                setMessage("Connexion perdue. Reconnexion...");
                break;
        }
    }
}

void GameScreen::sendClick(Pos p) {
    if (!net_) return;
    net::Action a;
    a.pos = p;
    switch (game_.phase()) {
        case Phase::PlaceBaseP1:
        case Phase::PlaceBaseP2:
            a.kind = net::ActionKind::Base;
            break;
        case Phase::PlaySquares:
            a.kind = net::ActionKind::Square;
            break;
        case Phase::RelocateBase: {
            Pos tl;
            a.kind = net::ActionKind::Relocate;
            a.pos  = findLegalTl(p, &tl) ? tl : p;
            break;
        }
        default:
            return;
    }
    net_->send(a);
}

void GameScreen::sendEndTurn() {
    if (!net_) return;
    net::Action a;
    a.kind = net::ActionKind::End;
    net_->send(a);
}

bool GameScreen::init() {
    const Board& b = game_.board();
    cellSize_ = std::max(6, std::min(28, 900 / std::max(b.width(), b.height())));

    const int w = std::max(b.width() * cellSize_, 560);
    const int h = b.height() * cellSize_ + static_cast<int>(kHudHeight);

    if (!SDL_CreateWindowAndRenderer("NervousGAME", w, h, 0, &window_, &renderer_)) {
        SDL_Log("CreateWindow: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    syncHp();
    lastTicks_ = SDL_GetTicks();

    // Purge les événements hérités du menu : évite un double clic fantôme.
    SDL_RaiseWindow(window_);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    startTicks_ = SDL_GetTicks();

    rebuildCache();

    if (online())
        setMessage("Connecte. En attente d'un adversaire...");
    else if (resumed_)
        setMessage("Partie reprise.");
    else
        setMessage("J1 : placez votre base.");

    // Un START deja arrive pendant la creation de la fenetre est traite ici.
    if (online()) pollNetwork();
    return true;
}

void GameScreen::setMode(GameMode m) {
    mode_ = m;
    aiLevel_ = (m == GameMode::VsComputerHard) ? AiLevel::Hard : AiLevel::Easy;
}

}  // namespace ng