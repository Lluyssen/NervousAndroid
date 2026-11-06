#pragma once
#include <vector>
#include "Board.h"
#include "Types.h"

namespace ng {

// Fonctions de règles pures : elles lisent le plateau sans le modifier.
    namespace rules {

        constexpr int kMinBaseGap = 5;        // R5 : 5 cases vides entre les bases au placement
        constexpr int kMinRelocateGap = 4;    // R4x : 4 cases vides minimum au déplacement
        constexpr int kMinGrid = 15;
        constexpr int kMaxGrid = 100;

// Base 2x2 dont (x,y) est le coin haut-gauche.
// Vrai si les 4 cases sont dans la grille.
        bool baseFitsInBoard(const Board& b, Pos topLeft);

// Vrai si les 4 cases de la base (x,y)-(x+1,y+1) sont toutes vides.
        bool baseAreaIsEmpty(const Board& b, Pos topLeft);

// Nombre de cases vides entre deux bases 2x2 (Chebyshev entre cases les plus proches, moins 1).
// Exemple : bases côte à côte (aucun écart) -> 0.
        int gapBetweenBases(Pos aTopLeft, Pos bTopLeft);

// Traversée (R52/R53 + T1/T2).
// Si on pose en "target" en venant de l'autre côté d'un mur adverse d'1 case dans la direction (dx,dy),
// renvoie vrai si la traversée est légale pour le joueur p.
// "target" est la case où p veut poser son carré.
        bool canCrossInto(const Board& b, Player p, Pos target);

// Vrai si le carré peut être posé en pos pour le joueur p (R19 + traversée).
// En cas d'échec, "why" indique la raison.
        bool canPlaceSquare(const Board& b, Player p, Pos pos, Result* why = nullptr);

// Nombre de dégâts infligés à la base adverse si p pose un carré en pos
// (R6 : contact côté ou coin avec la base adverse).
        int damageForPlacement(const Board& b, Player p, Pos pos);

// Vrai si la base du joueur "target" est accessible à l'adversaire (T4).
        bool isBaseAccessible(const Board& b, Player target);

// Vrai si la base du joueur "target" est enfermée (inverse de isBaseAccessible).
        inline bool isEnclosed(const Board& b, Player target) {
            return !isBaseAccessible(b, target);
        }

// Cherche le coin haut-gauche de la base du joueur p. Renvoie false si absente.
        bool findBase(const Board& b, Player p, Pos* topLeft);

// Liste des positions légales de déplacement de la base de p (T5).
        std::vector<Pos> legalRelocations(const Board& b, Player p);

// Vrai si le joueur p a au moins une pose de carré légale.
        bool hasAnyLegalPlacement(const Board& b, Player p);

    }  // namespace rules
}  // namespace ng