#include "GameScreen_Internal.h"

namespace ng {

Player GameScreen::currentActor() const {
    return game_.phase() == Phase::RelocateBase ? game_.mustRelocatePlayer()
                                                : game_.current();
}

bool GameScreen::isComputerTurn() const {
    return vsComputer() && !game_.isOver() && currentActor() == Player::Two;
}

void GameScreen::afterStateChange(Result r) {
    setMessage(describe(r));
    rebuildCache();
}

void GameScreen::startAiThinking() {
    if (aiBusy_.load()) return;
    if (aiThread_.joinable()) aiThread_.join();
    aiBusy_ = true;
    aiReady_ = false;

    Game snapshot = game_;
    aiThread_ = std::thread([this, snapshot]() {
        Move m = mcts_.chooseMove(snapshot, kAiThinkMs);
        aiMove_ = m;             // écrit avant la publication de aiReady_
        aiReady_.store(true);    // release : rend aiMove_ visible au thread principal
    });
}

void GameScreen::applyAiMove() {
    if (aiThread_.joinable()) aiThread_.join();
    aiBusy_ = false;
    aiReady_ = false;

    Result r = Result::WrongPhase;
    switch (aiMove_.kind) {
        case Move::Kind::Square:   r = game_.placeSquare(aiMove_.pos);  break;
        case Move::Kind::EndTurn:  r = game_.endTurn();                 break;
        case Move::Kind::Base:     r = game_.placeBase(aiMove_.pos);    break;
        case Move::Kind::Relocate: r = game_.relocateBase(aiMove_.pos); break;
    }

    if (r == Result::Ok) {
        afterStateChange(r);
    } else {
        // Le MCTS a proposé un coup refusé : un seul repli, sans boucle.
        fallbackAiMove();
    }
}

void GameScreen::fallbackAiMove() {
    Result r = Result::WrongPhase;

    switch (game_.phase()) {
        case Phase::PlaceBaseP1:
        case Phase::PlaceBaseP2: {
            auto cells = game_.legalInitialBasePlacements();
            if (!cells.empty()) {
                std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
                r = game_.placeBase(cells[d(rng_)]);
            }
            break;
        }
        case Phase::RelocateBase: {
            auto cells = game_.legalBaseRelocations();
            if (!cells.empty()) {
                std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
                r = game_.relocateBase(cells[d(rng_)]);
            }
            break;
        }
        case Phase::PlaySquares: {
            auto cells = game_.legalSquarePlacements();
            // Aucune pose possible : on termine le tour (sinon blocage).
            if (cells.empty()) {
                r = game_.endTurn();
                break;
            }
            std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
            r = game_.placeSquare(cells[d(rng_)]);
            break;
        }
        default:
            break;
    }

    // Garde-fou : tour en cours, rien joué, au moins une pose faite -> fin de tour.
    if (r != Result::Ok && game_.phase() == Phase::PlaySquares &&
        game_.placedThisTurn() >= 1) {
        r = game_.endTurn();
    }

    afterStateChange(r);
}

void GameScreen::playComputerTurn() {
    if (aiLevel_ == AiLevel::Easy) {
        // IA facile : coups légaux aléatoires, avec un délai pour qu'on les voie.
        const Uint64 now = SDL_GetTicks();
        if (now - lastEasyMoveTicks_ < kEasyDelayMs) return;
        lastEasyMoveTicks_ = now;
        fallbackAiMove();
        return;
    }

    // IA experte : MCTS asynchrone.
    if (aiReady_.load())      applyAiMove();
    else if (!aiBusy_.load()) startAiThinking();
}

void GameScreen::rebuildCache() {
    const Board& b = game_.board();
    const int W = b.width(), H = b.height();

    std::vector<Pos> cells;
    legalTlMask_.assign(static_cast<size_t>(W) * H, 0);
    hintMask_.assign(static_cast<size_t>(W) * H, 0);

    const Phase ph = game_.phase();
    const bool isBase = (ph == Phase::PlaceBaseP1 || ph == Phase::PlaceBaseP2 ||
                         ph == Phase::RelocateBase);

    switch (ph) {
        case Phase::PlaceBaseP1:
        case Phase::PlaceBaseP2:  cells = game_.legalInitialBasePlacements(); break;
        case Phase::RelocateBase: cells = game_.legalBaseRelocations();       break;
        case Phase::PlaySquares:  cells = game_.legalSquarePlacements();      break;
        default: break;
    }

    for (const Pos& p : cells) {
        if (p.x < 0 || p.y < 0 || p.x >= W || p.y >= H) continue;
        legalTlMask_[static_cast<size_t>(p.y) * W + p.x] = 1;

        if (isBase) {
            // Une base couvre 2x2 : on marque les 4 cases.
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const int cx = p.x + dx, cy = p.y + dy;
                    if (cx < W && cy < H)
                        hintMask_[static_cast<size_t>(cy) * W + cx] = 1;
                }
        } else {
            hintMask_[static_cast<size_t>(p.y) * W + p.x] = 1;
        }
    }
}

bool GameScreen::findLegalTl(Pos click, Pos* out) const {
    const int W = game_.board().width(), H = game_.board().height();
    static constexpr int kOffsets[2] = {0, -1};
    for (int dy : kOffsets)
        for (int dx : kOffsets) {
            const int tx = click.x + dx, ty = click.y + dy;
            if (tx < 0 || ty < 0 || tx >= W || ty >= H) continue;
            if (legalTlMask_[static_cast<size_t>(ty) * W + tx]) {
                *out = Pos{tx, ty};
                return true;
            }
        }
    return false;
}

void GameScreen::syncHp() {
    maxHp_ = std::max(1, std::max(game_.hp(Player::One), game_.hp(Player::Two)));
    prevHp_[0] = game_.hp(Player::One);
    prevHp_[1] = game_.hp(Player::Two);
    lostFlash_[0] = lostFlash_[1] = 0.f;
}

}  // namespace ng
