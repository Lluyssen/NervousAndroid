#include "Rules.h"
#include <algorithm>
#include <vector>

namespace ng {
namespace rules {

namespace {

const int DX4[4] = {1, -1, 0, 0};
const int DY4[4] = {0, 0, 1, -1};

// Vrai si la case (x,y) est un carré (pas une base) de l'adversaire de p.
bool isEnemySquare(const Board& b, Player p, int x, int y) {
    return b.inBounds(x, y) && b.isSquareOf(x, y, opponent(p));
}

// Vrai si (x,y) touche par un côté une case alliée (carré ou base) de p.
bool touchesAllySide(const Board& b, Player p, int x, int y) {
    for (int d = 0; d < 4; ++d) {
        int nx = x + DX4[d], ny = y + DY4[d];
        if (b.inBounds(nx, ny) && b.isOwnedBy(nx, ny, p)) return true;
    }
    return false;
}

// Épaisseur du mur adverse dans l'axe (dx,dy) : nombre de cases adverses
// consécutives en partant de (wx,wy) dans la direction (dx,dy), inclus.
int thicknessInAxis(const Board& b, Player p, int wx, int wy, int dx, int dy) {
    int count = 0;
    int x = wx, y = wy;
    while (b.inBounds(x, y) &&
           (b.get(x, y) == squareOf(opponent(p)) || b.get(x, y) == baseOf(opponent(p)))) {
        ++count;
        x += dx;
        y += dy;
    }
    return count;
}

// Traversée vers "target" dans la direction (dx,dy) : le mur est la case
// (target - d), d'épaisseur exactement 1 dans l'axe, et la case derrière
// le mur doit appartenir à p (R53).
bool crossesThinWallFrom(const Board& b, Player p, Pos target, int dx, int dy) {
    int wx = target.x - dx, wy = target.y - dy;   // case du mur
    int bx = wx - dx,       by = wy - dy;         // case de départ, derrière le mur
    if (!b.inBounds(wx, wy) || !b.inBounds(bx, by)) return false;
    if (!isEnemySquare(b, p, wx, wy)) return false;

    // Épaisseur exactement 1 : la case de départ ne doit pas être adverse.
    if (thicknessInAxis(b, p, wx, wy, -dx, -dy) != 1) return false;
    if (!b.isOwnedBy(bx, by, p)) return false;

    return touchesAllySide(b, p, wx, wy);
}

}  // namespace

bool baseFitsInBoard(const Board& b, Pos tl) {
    return tl.x >= 0 && tl.y >= 0 && tl.x + 1 < b.width() && tl.y + 1 < b.height();
}

bool baseAreaIsEmpty(const Board& b, Pos tl) {
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx)
            if (!b.isEmpty(tl.x + dx, tl.y + dy)) return false;
    return true;
}

int gapBetweenBases(Pos a, Pos bb) {
    // Base A couvre [a.x, a.x+1] x [a.y, a.y+1] ; idem B.
    auto gap1d = [](int a0, int b0) {
        int a1 = a0 + 1, b1 = b0 + 1;
        if (b0 > a1) return b0 - a1 - 1;   // B à droite/en bas de A
        if (a0 > b1) return a0 - b1 - 1;   // A à droite/en bas de B
        return -1;                         // chevauchement sur cet axe
    };
    int gx = gap1d(a.x, bb.x);
    int gy = gap1d(a.y, bb.y);
    if (gx < 0 && gy < 0) return -1;       // chevauchement
    return std::max(gx, gy);
}

bool canCrossInto(const Board& b, Player p, Pos target) {
    for (int d = 0; d < 4; ++d)
        if (crossesThinWallFrom(b, p, target, DX4[d], DY4[d])) return true;
    return false;
}

bool canPlaceSquare(const Board& b, Player p, Pos pos, Result* why) {
    auto fail = [&](Result r) { if (why) *why = r; return false; };
    if (!b.inBounds(pos)) return fail(Result::OutOfBounds);
    if (!b.isEmpty(pos.x, pos.y)) return fail(Result::OccupiedCell);

    // Contact par un côté avec un allié (R19).
    if (touchesAllySide(b, p, pos.x, pos.y)) {
        if (why) *why = Result::Ok;
        return true;
    }

    // Traversée d'un mur adverse d'une case (R53).
    if (canCrossInto(b, p, pos)) {
        if (why) *why = Result::Ok;
        return true;
    }

    // Diagnostic : un mur adverse épais est-il adjacent ?
    for (int d = 0; d < 4; ++d) {
        int wx = pos.x - DX4[d], wy = pos.y - DY4[d];
        if (isEnemySquare(b, p, wx, wy)) {
            int thick = thicknessInAxis(b, p, wx, wy, -DX4[d], -DY4[d]);
            if (thick >= 2) return fail(Result::ThickWall);
        }
    }
    return fail(Result::NotConnected);
}

int damageForPlacement(const Board& b, Player p, Pos pos) {
    // R6 : un carré qui touche la base adverse (côté ou coin) lui inflige 1 dégât.
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = pos.x + dx, ny = pos.y + dy;
            if (b.inBounds(nx, ny) && b.isBaseOf(nx, ny, opponent(p))) return 1;
        }
    return 0;
}

bool findBase(const Board& b, Player p, Pos* tl) {
    for (int y = 0; y < b.height(); ++y)
        for (int x = 0; x < b.width(); ++x)
            if (b.isBaseOf(x, y, p)) {
                if (tl) *tl = Pos{x, y};
                return true;
            }
    return false;
}

// Vrai si la base de "target" est atteignable par l'adversaire (protégée).
bool isBaseProtected(const Board& b, Player target) {
    return floodReaches(b, opponent(target), target);
}

// Vrai si la base de p est enfermée : aucun accès depuis l'extérieur.
bool isEnclosed(const Board& b, Player p) {
    return !isBaseProtected(b, p);
}

bool floodReaches(const Board& b, Player from, Player target) {
    const int W = b.width(), H = b.height();
    std::vector<char> seen(static_cast<size_t>(W) * H, 0);
    std::vector<Pos> stack;

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (b.get(x, y) == baseOf(from)) {
                seen[y * W + x] = 1;
                stack.push_back(Pos{x, y});
            }

    while (!stack.empty()) {
        Pos p = stack.back();
        stack.pop_back();

        // Contact côté ou coin avec la base cible.
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                int tx = p.x + dx, ty = p.y + dy;
                if (b.inBounds(tx, ty) && b.get(tx, ty) == baseOf(target)) return true;
            }

        for (int d = 0; d < 4; ++d) {
            int nx = p.x + DX4[d], ny = p.y + DY4[d];
            if (!b.inBounds(nx, ny) || seen[ny * W + nx]) continue;
            Cell c = b.get(nx, ny);
            if (c == Cell::Empty || c == squareOf(from)) {
                seen[ny * W + nx] = 1;
                stack.push_back(Pos{nx, ny});
                continue;
            }
            // Traversée : un carré adverse d'épaisseur 1 peut être franchi
            // si la case derrière n'est pas un autre carré adverse (R53).
            if (c == squareOf(target)) {
                int fx = nx + DX4[d], fy = ny + DY4[d];
                if (b.inBounds(fx, fy) &&
                    (b.get(fx, fy) == Cell::Empty || b.get(fx, fy) == squareOf(from)) &&
                    !seen[fy * W + fx]) {
                    seen[ny * W + nx] = 1;
                    seen[fy * W + fx] = 1;
                    stack.push_back(Pos{fx, fy});
                }
            }
        }
    }
    return false;
}

bool baseIsMutuallyReachable(const Board& b, Player p) {
    Pos other;
    if (!findBase(b, opponent(p), &other)) return true;   // pas encore de base adverse
    return floodReaches(b, opponent(p), p) && floodReaches(b, p, opponent(p));
}

std::vector<Pos> legalRelocations(const Board& b, Player p) {
    std::vector<Pos> result;
    Pos oldBase;
    if (!findBase(b, p, &oldBase)) return result;
    Pos enemyBase;
    if (!findBase(b, opponent(p), &enemyBase)) return result;

    for (int y = 0; y + 1 < b.height(); ++y)
        for (int x = 0; x + 1 < b.width(); ++x) {
            Pos tl{x, y};
            // L'ancienne base ne compte pas comme case libre pour la nouvelle position.
            bool overlapsOld = (x < oldBase.x + 2 && x + 2 > oldBase.x &&
                                y < oldBase.y + 2 && y + 2 > oldBase.y);
            if (overlapsOld) continue;
            if (!baseAreaIsEmpty(b, tl)) continue;
            if (gapBetweenBases(tl, enemyBase) < kMinRelocateGap) continue;

            // Même simulation que Game::relocateBase.
            Board copy = b;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx)
                    copy.set(oldBase.x + dx, oldBase.y + dy, squareOf(p));
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx)
                    copy.set(x + dx, y + dy, baseOf(p));

            if (baseIsMutuallyReachable(copy, p)) result.push_back(tl);
        }
    return result;
}

bool hasAnyLegalPlacement(const Board& b, Player p) {
    for (int y = 0; y < b.height(); ++y)
        for (int x = 0; x < b.width(); ++x)
            if (canPlaceSquare(b, p, Pos{x, y}, nullptr)) return true;
    return false;
}

}  // namespace rules
}  // namespace ng