#pragma once
#include <vector>
#include "Types.h"

namespace ng {

// Plateau : grille 1D de cases (D2).
    class Board {
    public:
        Board(int width, int height);

        int width() const { return w_; }
        int height() const { return h_; }

        bool inBounds(int x, int y) const {
            return x >= 0 && y >= 0 && x < w_ && y < h_;
        }
        bool inBounds(Pos p) const { return inBounds(p.x, p.y); }

        Cell get(int x, int y) const { return cells_[idx(x, y)]; }
        Cell get(Pos p) const { return get(p.x, p.y); }
        void set(int x, int y, Cell c) { cells_[idx(x, y)] = c; }
        void set(Pos p, Cell c) { set(p.x, p.y, c); }

        bool isEmpty(int x, int y) const { return get(x, y) == Cell::Empty; }

        // Vrai si la case contient un carré ou une base du joueur p.
        bool isOwnedBy(int x, int y, Player p) const;
        bool isSquareOf(int x, int y, Player p) const;
        bool isBaseOf(int x, int y, Player p) const;

    private:
        int idx(int x, int y) const { return y * w_ + x; }
        int w_, h_;
        std::vector<Cell> cells_;
    };

}  // namespace ng