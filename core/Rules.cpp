#include "Rules.h"
#include <algorithm>
#include <cstdlib>
#include <queue>

namespace ng {
    namespace rules {

        namespace {

            const int DX4[4] = {1, -1, 0, 0};
            const int DY4[4] = {0, 0, 1, -1};

            // Vrai si la case (x,y) est un carrÃĐ (pas une base) de l'adversaire de p.
            bool isEnemySquare(const Board& b, Player p, int x, int y) {
                return b.inBounds(x, y) && b.isSquareOf(x, y, opponent(p));
            }

            // Vrai si (x,y) touche par un COTÃ une case alliÃĐe (carrÃĐ ou base) de p.
            bool touchesAllySide(const Board& b, Player p, int x, int y) {
                for (int d = 0; d < 4; ++d) {
                    int nx = x + DX4[d], ny = y + DY4[d];
                    if (b.inBounds(nx, ny) && b.isOwnedBy(nx, ny, p)) return true;
                }
                return false;
            }

            // Ãpaisseur du mur adverse dans l'axe (dx,dy) : nombre de cases adverses
            // consÃĐcutives en partant de (wx,wy) dans la direction (dx,dy), inclus.
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

            // TraversÃĐe depuis "target" : on regarde vers l'arriÃĻre dans chaque direction.
            // Le mur est la case (target - d). Il doit ÃŠtre un carrÃĐ adverse d'ÃĐpaisseur 1
            // dans l'axe de passage, et toucher par un COTÃ un alliÃĐ de p (R53).
            bool crossesThinWallFrom(const Board& b, Player p, Pos target, int dx, int dy) {
                int wx = target.x - dx, wy = target.y - dy;       // case du mur
                int bx = wx - dx, by = wy - dy;                    // case de dÃĐpart (derriÃĻre le mur)
                if (!b.inBounds(wx, wy) || !b.inBounds(bx, by)) return false;
                if (!isEnemySquare(b, p, wx, wy)) return false;

                // Ãpaisseur dans l'axe : exactement 1 case (la case derriÃĻre doit ÃŠtre non adverse).
                int thick = thicknessInAxis(b, p, wx, wy, -dx, -dy);
                if (thick != 1) return false;

                // R53 : le mur doit toucher par un cÃītÃĐ un carrÃĐ/base alliÃĐ (la case "derriÃĻre"
                // en est la candidate naturelle, mais n'importe quel cÃītÃĐ du mur compte).
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
                if (b0 > a1) return b0 - a1 - 1;   // B Ã  droite/en bas de A
                if (a0 > b1) return a0 - b1 - 1;   // A Ã  droite/en bas de B
                return -1;                         // chevauchement sur cet axe
            };
            int gx = gap1d(a.x, bb.x);
            int gy = gap1d(a.y, bb.y);
            // Distance de Chebyshev entre cases les plus proches = max des ÃĐcarts par axe.
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

            // Cas normal : contact par un COTÃ avec un alliÃĐ (R19).
            if (touchesAllySide(b, p, pos.x, pos.y)) return true;

            // Cas traversÃĐe (R53) : de l'autre cÃītÃĐ d'un mur adverse d'1 case.
            if (canCrossInto(b, p, pos)) return true;

            // Diagnostic : y a-t-il un mur adverse ÃĐpais juste Ã  cÃītÃĐ ?
            for (int d = 0; d < 4; ++d) {
                int wx = pos.x - DX4[d], wy = pos.y - DY4[d];
                if (b.inBounds(wx, wy) && isEnemySquare(b, p, wx, wy)) {
                    int thick = thicknessInAxis(b, p, wx, wy, -DX4[d], -DY4[d]);
                    if (thick >= 2) return fail(Result::ThickWall);
                }
            }
            return fail(Result::NotConnected);
        }

        int damageForPlacement(const Board& b, Player p, Pos pos) {
            // R6 : un carrÃĐ qui touche la base adverse (cÃītÃĐ ou coin) lui inflige 1 dÃĐgÃĒt.
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

        bool isBaseAccessible(const Board& b, Player target) {
            const Player attacker = opponent(target);
            const int W = b.width(), H = b.height();

            // Contact direct : une case touche (cÃītÃĐ ou coin) la base cible.
            auto touchesTargetBase = [&](const Board& bb, int x, int y) {
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (!dx && !dy) continue;
                        int nx = x + dx, ny = y + dy;
                        if (bb.inBounds(nx, ny) && bb.isBaseOf(nx, ny, target))
                            return true;
                    }
                return false;
            };

            Board sim = b;                       // copie : on y pose des carrÃĐs virtuels
            std::queue<Pos> frontier;

            // Sources : toutes les cases de l'attaquant (base incluse, car au dÃĐpart
            // il n'a aucun carrÃĐ). Le contact direct ne compte que pour un carrÃĐ (R6).
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if (sim.isOwnedBy(x, y, attacker)) {
                        if (sim.isSquareOf(x, y, attacker) && touchesTargetBase(sim, x, y))
                            return true;
                        frontier.push(Pos{x, y});
                    }

            // Propagation : chaque case regarde les cases proches (losange de rayon 2 :
            // 4 directions + saut de 2 pour couvrir la traversÃĐe d'un mur d'1 case)
            // et pose si c'est lÃĐgal.
            while (!frontier.empty()) {
                Pos c = frontier.front();
                frontier.pop();
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx) {
                        if (!dx && !dy) continue;
                        if (std::abs(dx) + std::abs(dy) > 2) continue;
                        Pos n{c.x + dx, c.y + dy};
                        if (!sim.inBounds(n) || !sim.isEmpty(n.x, n.y)) continue;
                        if (!canPlaceSquare(sim, attacker, n, nullptr)) continue;

                        sim.set(n.x, n.y, squareOf(attacker));   // pose virtuelle
                        if (touchesTargetBase(sim, n.x, n.y)) return true;
                        frontier.push(n);
                    }
            }
            return false;
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
            if (!baseAreaIsEmpty(b, tl)) continue;
            int gap = gapBetweenBases(tl, enemyBase);
            if (gap < kMinRelocateGap) continue;

            // MÃŠme simulation que Game::relocateBase :
            // l'ancienne base reste sur le plateau en carrÃĐs ordinaires.
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

        bool floodReaches(const Board& b, Player from, Player target) {
    const int W = b.width(), H = b.height();
    std::vector<char> seen(W * H, 0);
    std::vector<Pos> stack;

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (b.get(x, y) == baseOf(from)) {
                seen[y * W + x] = 1;
                stack.push_back(Pos{x, y});
            }

    while (!stack.empty()) {
        Pos p = stack.back(); stack.pop_back();
        for (int d = 0; d < 4; ++d) {
            int nx = p.x + DX4[d], ny = p.y + DY4[d];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            if (seen[ny * W + nx]) continue;
            Cell c = b.get(nx, ny);
            if (c == baseOf(target)) return true;
            if (c == Cell::Empty || c == squareOf(from)) {
                seen[ny * W + nx] = 1;
                stack.push_back(Pos{nx, ny});
            }
        }
    }
    return false;
}

bool baseIsMutuallyReachable(const Board& b, Player p) {
    Pos other;
    if (!findBase(b, opponent(p), &other)) return true;
    return isBaseAccessible(b, p) && isBaseAccessible(b, opponent(p));
}

    }  // namespace rules
}  // namespace ng