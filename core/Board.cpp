#include "Board.h"

namespace ng {

    Board::Board(int width, int height)
            : w_(width), h_(height), cells_(static_cast<size_t>(width) * height, Cell::Empty) {}

    bool Board::isSquareOf(int x, int y, Player p) const {
        return get(x, y) == squareOf(p);
    }

    bool Board::isBaseOf(int x, int y, Player p) const {
        return get(x, y) == baseOf(p);
    }

    bool Board::isOwnedBy(int x, int y, Player p) const {
        return isSquareOf(x, y, p) || isBaseOf(x, y, p);
    }

}  // namespace ng