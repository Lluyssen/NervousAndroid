#pragma once
#include <vector>
#include "Board.h"
#include "Rules.h"
#include "Types.h"

namespace ng {

// Phases d'une partie.
    enum class Phase {
        PlaceBaseP1,      // J1 place sa base
        PlaceBaseP2,      // J2 place sa base
        PlaySquares,      // tour normal : poser 1 ou 2 carrés
        RelocateBase,     // une base enfermée doit être déplacée (gratuit)
        GameOver
    };

    class Game {
    public:
        // Largeur et hauteur entre 15 et 100 (R50). Lance std::invalid_argument sinon.
        Game(int width, int height);

        // --- Lecture de l'état ---
        const Board& board() const { return board_; }
        Phase phase() const { return phase_; }
        Player current() const { return current_; }       // joueur dont c'est le tour
        int hp(Player p) const { return hp_[static_cast<int>(p)]; }
        int placedThisTurn() const { return placed_; }
        bool isOver() const { return phase_ == Phase::GameOver; }
        Player winner() const { return winner_; }          // valable si isOver()
        Player mustRelocatePlayer() const { return relocating_; }

        // --- Actions ---
        // Placement initial d'une base (phases PlaceBaseP1 / PlaceBaseP2).
        Result placeBase(Pos topLeft);

        // Pose d'un carré (phase PlaySquares).
        Result placeSquare(Pos pos);

        // Fin du tour : exige au moins 1 pose (R49).
        Result endTurn();

        // Déplacement d'une base enfermée (phase RelocateBase). Gratuit (R43).
        Result relocateBase(Pos topLeft);

        // --- Aides pour l'interface ---
        std::vector<Pos> legalSquarePlacements() const;
        std::vector<Pos> legalBaseRelocations() const;
        // Placements légaux de la base du joueur courant en phase de mise en place.
        std::vector<Pos> legalInitialBasePlacements() const;

    private:
        void afterPlacementChecks();          // dégâts déjà appliqués ; vérifie enfermement + blocage
        void finish(Player winner);
        void checkEnclosures();               // après chaque pose (E1)

        Board board_;
        Phase phase_ = Phase::PlaceBaseP1;
        Player current_ = Player::One;
        Player winner_ = Player::One;
        Player relocating_ = Player::One;
        int hp_[2] = {4, 4};
        int placed_ = 0;
    };

}  // namespace ng