#include "Game.h"
#include <stdexcept>
#include <fstream>

namespace ng {

// ---------------------------------------------------------------------------
// Sauvegarde / chargement
// ---------------------------------------------------------------------------

bool Game::saveToFile(const std::string& path, int mode) const {
    std::ofstream f(path);
    if (!f) return false;

    f << "NERVOUS_SAVE 1\n";
    f << "mode " << mode << "\n";
    f << "size " << board_.width() << " " << board_.height() << "\n";
    f << "phase " << static_cast<int>(phase_) << "\n";
    f << "current " << static_cast<int>(current_) << "\n";
    f << "relocating " << static_cast<int>(relocating_) << "\n";
    f << "hp " << hp_[0] << " " << hp_[1] << "\n";
    f << "placed " << placed_ << "\n";
    f << "winner " << (winner_.has_value() ? static_cast<int>(*winner_) : -1) << "\n";
    f << "cells\n";
    for (int y = 0; y < board_.height(); ++y) {
        for (int x = 0; x < board_.width(); ++x)
            f << static_cast<int>(board_.get(x, y)) << ' ';
        f << '\n';
    }
    return static_cast<bool>(f);
}

std::optional<Game> Game::loadFromFile(const std::string& path, int* modeOut) {
    std::ifstream f(path);
    if (!f) return std::nullopt;

    std::string tag, key;
    int version = 0, mode = 0, w = 0, h = 0;
    int phase = 0, cur = 0, reloc = 0, hp0 = 0, hp1 = 0, placed = 0, win = -1;

    if (!(f >> tag >> version) || tag != "NERVOUS_SAVE" || version != 1) return std::nullopt;
    if (!(f >> key >> mode)             || key != "mode")       return std::nullopt;
    if (!(f >> key >> w >> h)           || key != "size")       return std::nullopt;
    if (!(f >> key >> phase)            || key != "phase")      return std::nullopt;
    if (!(f >> key >> cur)              || key != "current")    return std::nullopt;
    if (!(f >> key >> reloc)            || key != "relocating") return std::nullopt;
    if (!(f >> key >> hp0 >> hp1)       || key != "hp")         return std::nullopt;
    if (!(f >> key >> placed)           || key != "placed")     return std::nullopt;
    if (!(f >> key >> win)              || key != "winner")     return std::nullopt;
    if (!(f >> key)                     || key != "cells")      return std::nullopt;

    if (phase < 0 || phase > static_cast<int>(Phase::GameOver)) return std::nullopt;
    if (cur < 0 || cur > 1 || reloc < 0 || reloc > 1)           return std::nullopt;

    try {
        Game g(w, h);   // valide la taille (15..100)
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                int c = 0;
                if (!(f >> c) || c < 0 || c > static_cast<int>(Cell::Base2))
                    return std::nullopt;
                g.board_.set(x, y, static_cast<Cell>(c));
            }

        g.phase_      = static_cast<Phase>(phase);
        g.current_    = static_cast<Player>(cur);
        g.relocating_ = static_cast<Player>(reloc);
        g.hp_[0]      = hp0;
        g.hp_[1]      = hp1;
        g.placed_     = placed;
        if (win == 0 || win == 1) g.winner_ = static_cast<Player>(win);

        if (modeOut) *modeOut = mode;
        return g;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// ---------------------------------------------------------------------------
// Messages
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Construction / fin de partie
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Aides pour l'interface
// ---------------------------------------------------------------------------

std::vector<Pos> Game::legalSquarePlacementsFor(Player p) const {
    std::vector<Pos> out;
    for (int y = 0; y < board_.height(); ++y)
        for (int x = 0; x < board_.width(); ++x) {
            Pos c{x, y};
            if (rules::canPlaceSquare(board_, p, c, nullptr)) out.push_back(c);
        }
    return out;
}

std::vector<Pos> Game::legalSquarePlacements() const {
    if (phase_ != Phase::PlaySquares) return {};
    return legalSquarePlacementsFor(current_);
}

std::vector<Pos> Game::legalBaseRelocations() const {
    if (phase_ != Phase::RelocateBase) return {};
    return rules::legalRelocations(board_, relocating_);
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

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

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

// Logique interne
void Game::checkEnclosures() {
    // E1 : vérification après chaque pose, pour les deux bases.
    // Une base doit être déplacée si l'adversaire ne peut plus l'atteindre.
    Player order[2] = {current_, opponent(current_)};
    for (Player p : order) {
        // Pas de vérification tant que les deux bases ne sont pas posées.
        if (!rules::findBase(board_, p, nullptr) ||
            !rules::findBase(board_, opponent(p), nullptr))
            continue;

        if (!rules::isEnclosed(board_, p)) continue;

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

}  // namespace ng