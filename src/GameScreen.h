#pragma once
#include <SDL3/SDL.h>
#include <atomic>
#include <random>
#include <string>
#include <thread>
#include <memory>
#include <vector>
#include "../core/Game.h"
#include "MenuScreen.h"
#include "MctsAi.h"
#include "../reseau/NetClient.h"
#include "../reseau/Protocol.h"

namespace ng {

class GameScreen {
public:
    GameScreen(int width, int height);
    GameScreen(Game loaded, GameMode mode);   // reprise d'une partie sauvegardee
    ~GameScreen();

    GameScreen(const GameScreen&) = delete;
    GameScreen& operator=(const GameScreen&) = delete;

    bool init();
    void run();
    void setMode(GameMode m);
    bool connectOnline(const std::string& host, int port);
    bool wantsMenu() const { return wantsMenu_; }

private:
    enum class AiLevel { Easy, Hard };

    // --- Etat general ---
    bool dirty_ = true;                 // conserve pour compatibilite, non requis
    std::vector<char> legalTlMask_;     // lookup O(1) : coin haut-gauche legal ?
    std::vector<char> hintMask_;        // cases a marquer visuellement

    GameMode mode_       = GameMode::LocalOneVsOne;
    AiLevel  aiLevel_    = AiLevel::Easy;
    Uint64   startTicks_ = 0;

    // --- Reseau ---
    std::unique_ptr<net::NetClient> net_;
    Player  myPlayer_      = Player::One;
    bool    gameStarted_   = false;   // faux tant que START n'est pas recu
    bool    opponentLeft_  = false;
    bool    connectionLost_ = false;

    // En ligne = une connexion reseau existe (modes Host et Join).
    // Apres un echec de connexion, net_ est remis a zero : online() devient faux.
    bool online() const { return net_ != nullptr; }
    bool canAct() const;
    void pollNetwork();
    void sendClick(Pos p);
    void sendEndTurn();

    bool vsComputer() const {
        return mode_ == GameMode::VsComputerEasy ||
               mode_ == GameMode::VsComputerHard;
    }

    // --- Helpers d'etat (point unique de verite) ---
    Player currentActor() const;        // joueur qui doit agir (gere RelocateBase)
    bool   isComputerTurn() const;      // vrai si c'est a l'ordinateur de jouer
    void   afterStateChange(Result r);  // message + recalcul du cache apres coup

    // --- IA ---
    std::mt19937 rng_{std::random_device{}()};
    MctsAi mcts_;                        // IA experte (Monte-Carlo Tree Search)
    std::thread aiThread_;               // reflexion en arriere-plan
    std::atomic<bool> aiBusy_{false};    // reflexion en cours
    std::atomic<bool> aiReady_{false};   // coup pret a etre applique
    Move aiMove_;                        // resultat du MCTS (ecrit avant aiReady_)
    Uint64 lastEasyMoveTicks_ = 0;       // delai visuel pour l'IA facile

    void playComputerTurn();
    void startAiThinking();
    void applyAiMove();
    void fallbackAiMove();               // coup legal aleatoire (IA facile / secours)

    // --- Cache des coups legaux ---
    void rebuildCache();                 // appeler uniquement apres un changement d'etat
    bool findLegalTl(Pos click, Pos* out) const;

    // --- Evenements / rendu ---
    void handleEvent(const SDL_Event& e);
    void handleClick(float mx, float my);
    void render();

    void drawGrid();
    void drawHighlights();
    void drawHud();

    bool pixelToCell(float mx, float my, Pos* out) const;
    void setMessage(const std::string& m) { message_ = m; }

    void syncHp();

    Game game_;
    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    int  cellSize_ = 24;
    bool running_  = true;
    std::string message_;

    // --- Interface moderne ---
    int    maxHp_ = 1;                  // PV max, lu a l'init
    Uint64 lastTicks_ = 0;
    float  time_ = 0.f;                 // temps cumule pour les pulsations

    float lostFlash_[2] = {0.f, 0.f};   // flash rouge au PV perdu
    int   prevHp_[2]    = {-1, -1};     // PV vus a la frame precedente

    void drawRoundedRect(float x, float y, float w, float h, float radius);
    void drawText(float x, float y, const std::string& s, float scale);
    void drawPlayerCard(Player pl, float x, float y, float w, float h);
    void drawStatusBar(float w);
    void drawGameOver();
    void updateAnimations();
    std::string playerName(Player pl) const;
    void drawMessageToast();

    // --- Reconnexion reseau ---
    std::string host_;
    int         port_ = 0;
    Uint64      nextRetryTicks_ = 0;
    int         retryCount_ = 0;
    bool        replaying_ = false;
    void tryReconnect();

    // --- Reprise de partie + pause ---
    bool   resumed_       = false;      // partie chargee depuis une sauvegarde
    bool   paused_        = false;
    bool   wantsMenu_     = false;      // lu par main.cpp pour revenir au menu
    int    pauseHover_    = -1;
    Uint64 savedMsgUntil_ = 0;          // affichage "Partie sauvegardee"

    struct PauseButton { float x, y, w, h; const char* label; };
    PauseButton pauseButton(int i) const;   // 0 = Sauvegarder, 1 = Retour Menu
    int  pauseButtonAt(float mx, float my) const;
    void drawPauseMenu();
    void handlePauseClick(float mx, float my);
};

}  // namespace ng