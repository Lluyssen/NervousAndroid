#pragma once
#include <string>

namespace ng {

// Les deux joueurs. J1 = rouge, J2 = bleu (R54).
    enum class Player { One = 0, Two = 1 };

    inline Player opponent(Player p) {
        return p == Player::One ? Player::Two : Player::One;
    }

// Contenu d'une case.
    enum class Cell : unsigned char {
        Empty,
        Square1,   // carré du joueur 1 (rouge)
        Square2,   // carré du joueur 2 (bleu)
        Base1,     // case occupée par la base du joueur 1
        Base2      // case occupée par la base du joueur 2
    };

    inline Cell squareOf(Player p) {
        return p == Player::One ? Cell::Square1 : Cell::Square2;
    }
    inline Cell baseOf(Player p) {
        return p == Player::One ? Cell::Base1 : Cell::Base2;
    }

    struct Pos {
        int x = 0;
        int y = 0;
        bool operator==(const Pos& o) const { return x == o.x && y == o.y; }
    };

// Résultat d'une tentative d'action : sert à expliquer les refus (D4).
    enum class Result {
        Ok,
        GameOver,
        WrongPhase,          // action non permise dans la phase actuelle
        OutOfBounds,         // hors de la grille
        OccupiedCell,        // case déjà occupée
        NotConnected,        // ne touche ni carré allié, ni base alliée, ni traversée valide
        ThickWall,           // mur adverse d'épaisseur >= 2 (ou coude)
        BaseTooClose,        // base trop proche de la base adverse
        BaseOverlap,         // base qui chevauche quelque chose
        BaseNotAccessible,   // base déplacée inaccessible à l'adversaire
        MustRelocateBase,    // la base est enfermée : déplacement obligatoire
        MaxPlacementsReached,// déjà 2 carrés posés ce tour
        NeedAtLeastOne       // fin de tour sans aucune pose (interdit, R49)
    };

    std::string describe(Result r);

}  // namespace ng