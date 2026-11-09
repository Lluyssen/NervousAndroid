#include "Game.h"
#include <stdexcept>

namespace ng {

std::string describe(Result r) {
    switch (r) {
        case Result::Ok: return "Action valide.";
        case Result::GameOver: return "La partie est terminée.";
        case Result::WrongPhase: return "Cette action n'est pas possible maintenant.";
        case Result::OutOfBounds: return "Case hors de la grille.";
        case Result::OccupiedCell: return "Cette case est déjà occupée.";
        case Result::NotConnected:
            return "Le carré doit toucher un de vos carrés ou votre base par un côté.";
        case Result::ThickWall:
            return "Mur adverse trop épais : on ne traverse qu'un mur d'une seule case.";
        case Result::BaseTooClose: return "Base trop proche de la base adverse.";
        case Result::BaseOverlap: return "La base doit tenir sur 4 cases libres dans la grille.";
        case Result::BaseNotAccessible:
            return "Cette position rendrait la base inaccessible à l'adversaire.";
        case Result::MustRelocateBase:
            return "Votre base est enfermée : vous devez la déplacer.";
        case Result::MaxPlacementsReached: return "Vous avez déjà posé 2 carrés ce tour.";
        case Result::NeedAtLeastOne: return "Vous devez poser au moins 1 carré.";
    }
    return "";
}

Game::Game(int width, int height) : board_(width, height) {
    if (width < rules::kMinGrid || width > rules::kMaxGrid ||
        height < rules::kMinGrid || height > rules::kMaxGrid)
        throw std::invalid_argument("La grille doit faire entre 15x15 et 100x100.");
}

void Game::finish(Player winner) {
    winner_ = winner;
    phase_ = Phase::GameOver;
}

bool Game::baseWouldBeReachable(const Board& candidate, Player p) const {
    return rules::baseIsMutuallyReachable(candidate, p);
}

std::vector<Pos> Game::legalSquarePlacementsFor(Player p) const {
    std::vector<Pos> out;
    for (int y = 0; y < board_.height(); ++y)
        for (int x = 0; x < board_.width(); ++x) {
            Pos c{x, y};
            if (rules::canPlaceSquare(board_, p, c, nullptr)) out.push_back(c);
        }
    return out;
}

std::vector<Pos> Game::legalInitialBasePlacements() const {
    std::vector<Pos> out;
    Pos other;
    bool hasOther = rules::findBase(board_, opponent(current_), &other);
    for (int y = 0; y + 1 < board_.height(); ++y)
        for (int x = 0; x + 1 < board_.width(); ++x) {
            Pos tl{x, y};
            if (!rules::baseAreaIsEmpty(board_, tl)) continue;
            if (hasOther && rules::gapBetweenBases(tl, other) < rules::kMinBaseGap) continue;

            Board copy = board_;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx)
                    copy.set(x + dx, y + dy, baseOf(current_));
            if (!baseWouldBeReachable(copy, current_)) continue;

            out.push_back(tl);
        }
    return out;
}

Result Game::placeBase(Pos tl) {
    if (isOver()) return Result::GameOver;
    if (phase_ != Phase::PlaceBaseP1 && phase_ != Phase::PlaceBaseP2) return Result::WrongPhase;
    if (!rules::baseFitsInBoard(board_, tl)) return Result::BaseOverlap;
    if (!rules::baseAreaIsEmpty(board_, tl)) return Result::BaseOverlap;

    Pos other;
    if (rules::findBase(board_, opponent(current_), &other) &&
        rules::gapBetweenBases(tl, other) < rules::kMinBaseGap)
        return Result::BaseTooClose;

    Board copy = board_;
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx)
            copy.set(tl.x + dx, tl.y + dy, baseOf(current_));
    if (!baseWouldBeReachable(copy, current_)) return Result::BaseNotAccessible;

    board_ = copy;

    if (phase_ == Phase::PlaceBaseP1) {
        phase_ = Phase::PlaceBaseP2;
        current_ = Player::Two;
    } else {
        phase_ = Phase::PlaySquares;
        current_ = Player::One;
        placed_ = 0;
    }
    return Result::Ok;
}

std::vector<Pos> Game::legalSquarePlacements() const {
    std::vector<Pos> out;
    if (phase_ != Phase::PlaySquares) return out;
    return legalSquarePlacementsFor(current_);
}

std::vector<Pos> Game::legalBaseRelocations() const {
    if (phase_ != Phase::RelocateBase) return {};
    return rules::legalRelocations(board_, relocating_);
}

Result Game::placeSquare(Pos pos) {
    if (isOver()) return Result::GameOver;
    if (phase_ == Phase::RelocateBase) return Result::MustRelocateBase;
    if (phase_ != Phase::PlaySquares) return Result::WrongPhase;
    if (placed_ >= 2) return Result::MaxPlacementsReached;

    Result why = Result::Ok;
    if (!rules::canPlaceSquare(board_, current_, pos, &why)) return why;

    board_.set(pos, squareOf(current_));
    ++placed_;

    // R6 : dégâts immédiats sur la base adverse.
    int dmg = rules::damageForPlacement(board_, current_, pos);
    if (dmg > 0) {
        int& enemyHp = hp_[static_cast<int>(opponent(current_))];
        enemyHp -= dmg;
        if (enemyHp <= 0) {
            enemyHp = 0;
            finish(current_);
            return Result::Ok;
        }
    }

    afterPlacementChecks();
    return Result::Ok;
}

void Game::checkEnclosures() {
    // E1 : vérification après chaque pose, pour les deux bases.
    // Une base doit être déplacée si elle est enfermée OU non mutuellement accessible.
    Player order[2] = {current_, opponent(current_)};
    for (Player p : order) {
        bool mustMove = rules::isEnclosed(board_, p)
                     || !rules::baseIsMutuallyReachable(board_, p);
        if (!mustMove) continue;

        if (rules::legalRelocations(board_, p).empty()) {
            // Aucune position légale : le propriétaire perd (F4 / R44).
            finish(opponent(p));
            return;
        }
        relocating_ = p;
        phase_ = Phase::RelocateBase;
        return;
    }
}

void Game::afterPlacementChecks() {
    checkEnclosures();
    if (isOver() || phase_ == Phase::RelocateBase) return;

    // R49/F : si le joueur courant a posé ses 2 carrés, le tour passe.
    if (placed_ >= 2) {
        current_ = opponent(current_);
        placed_ = 0;
        if (!rules::hasAnyLegalPlacement(board_, current_)) finish(opponent(current_));
    }
}

Result Game::endTurn() {
    if (isOver()) return Result::GameOver;
    if (phase_ == Phase::RelocateBase) return Result::MustRelocateBase;
    if (phase_ != Phase::PlaySquares) return Result::WrongPhase;
    if (placed_ < 1) return Result::NeedAtLeastOne;   // R49

    current_ = opponent(current_);
    placed_ = 0;
    if (!rules::hasAnyLegalPlacement(board_, current_)) finish(opponent(current_));
    return Result::Ok;
}

Result Game::relocateBase(Pos tl) {
    if (isOver()) return Result::GameOver;
    if (phase_ != Phase::RelocateBase) return Result::WrongPhase;
    if (!rules::baseFitsInBoard(board_, tl)) return Result::BaseOverlap;

    Pos oldBase, enemyBase;
    rules::findBase(board_, relocating_, &oldBase);
    rules::findBase(board_, opponent(relocating_), &enemyBase);

    if (!rules::baseAreaIsEmpty(board_, tl)) return Result::BaseOverlap;
    if (rules::gapBetweenBases(tl, enemyBase) < rules::kMinRelocateGap)
        return Result::BaseTooClose;

    // L'ancienne base devient des carrés ordinaires, puis on pose la nouvelle base.
    Board copy = board_;
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx)
            copy.set(oldBase.x + dx, oldBase.y + dy, squareOf(relocating_));
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx)
            copy.set(tl.x + dx, tl.y + dy, baseOf(relocating_));

    if (!rules::baseIsMutuallyReachable(copy, relocating_)) return Result::BaseNotAccessible;

    board_ = copy;   // déplacement validé, PV conservés
    phase_ = Phase::PlaySquares;

    // Le joueur qui a déplacé reprend la main ; on revérifie les enfermements.
    checkEnclosures();
    if (!isOver() && phase_ == Phase::PlaySquares) {
        if (placed_ >= 2) {
            current_ = opponent(current_);
            placed_ = 0;
        }
        if (placed_ == 0 && !rules::hasAnyLegalPlacement(board_, current_))
            finish(opponent(current_));
    }
    return Result::Ok;
}

}  // namespace ng