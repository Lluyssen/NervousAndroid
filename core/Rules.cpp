#include "Rules.h"
#include <algorithm>
#include <cstdlib>
#include <queue>

namespace ng {
    namespace rules {

        namespace {

            const int DX4[4] = {1, -1, 0, 0};
            const int DY4[4] = {0, 0, 1, -1};

// Vrai si la case (x,y) est un carré (pas une base) de l'adversaire de p.
            bool isEnemySquare(const Board& b, Player p, int x, int y) {
                return b.inBounds(x, y) && b.isSquareOf(x, y, opponent(p));
            }

// Vrai si (x,y) touche par un COTÉ une case alliée (carré ou base) de p.
            bool touchesAllySide(const Board& b, Player p, int x, int y) {
                for (int d = 0; d < 4; ++d) {
                    int nx = x + DX4[d], ny = y + DY4[d];
                    if (b.inBounds(nx, ny) && b.isOwnedBy(nx, ny, p)) return true;
                }
                return false;
            }

// Vrai si (x,y) touche par un côté OU un coin une case alliée de p.
            bool touchesAllyAny(const Board& b, Player p, int x, int y) {
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx, ny = y + dy;
                        if (b.inBounds(nx, ny) && b.isOwnedBy(nx, ny, p)) return true;
                    }
                return false;
            }

// Vérifie si la case adverse (wx,wy) est un mur d'épaisseur 1 traversable
// dans la direction d (la traversée va de l'arrière vers l'avant).
// T1 : aucun voisin orthogonal adverse (carré ou base) autour du mur.
// On interdit donc le coude et l'épaisseur 2.
            bool isThinWall(const Board& b, Player p, int wx, int wy) {
                if (!isEnemySquare(b, p, wx, wy)) return false;  // une base n'est pas traversable (T3)
                for (int d = 0; d < 4; ++d) {
                    int nx = wx + DX4[d], ny = wy + DY4[d];
                    if (!b.inBounds(nx, ny)) continue;
                    Cell c = b.get(nx, ny);
                    if (c == squareOf(opponent(p)) || c == baseOf(opponent(p))) {
                        // Un voisin adverse en prolongement du mur = épaisseur 2 ou coude.
                        // Mais un mur droit (rangée d'1 case de large) a des voisins adverses
                        // le long de la rangée : ils ne comptent que dans l'axe de traversée.
                        // On filtre donc dans canCrossInto (voir ci-dessous).
                    }
                }
                return true;
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

// Coude : le mur (wx,wy) a un voisin adverse sur l'axe perpendiculaire ET
// ce voisin a lui-même un voisin adverse dans l'axe de traversée (forme en L)
// -> épaisseur 2 dans l'axe de passage (R52).
            bool formsElbow(const Board& b, Player p, int wx, int wy, int dx, int dy) {
                // Axe perpendiculaire à (dx,dy).
                int px = dy != 0 ? 1 : 0;
                int py = dx != 0 ? 1 : 0;
                for (int s = -1; s <= 1; s += 2) {
                    int nx = wx + px * s, ny = wy + py * s;
                    if (!b.inBounds(nx, ny)) continue;
                    Cell c = b.get(nx, ny);
                    if (c != squareOf(opponent(p)) && c != baseOf(opponent(p))) continue;
                    // Le voisin perpendiculaire a-t-il un prolongement adverse dans l'axe de traversée ?
                    for (int t = -1; t <= 1; t += 2) {
                        int ax = nx + dx * t, ay = ny + dy * t;
                        if (!b.inBounds(ax, ay)) continue;
                        Cell a = b.get(ax, ay);
                        if (a == squareOf(opponent(p)) || a == baseOf(opponent(p))) return true;
                    }
                }
                return false;
            }

// Traversée depuis "target" : on regarde vers l'arrière dans chaque direction.
// Le mur est la case (target - d). Il doit être un carré adverse d'épaisseur 1
// dans l'axe, sans coude, et toucher par un COTÉ un allié de p (R53).
            bool crossesThinWallFrom(const Board& b, Player p, Pos target, int dx, int dy) {
                int wx = target.x - dx, wy = target.y - dy;       // case du mur
                int bx = wx - dx, by = wy - dy;                    // case de départ (derrière le mur)
                if (!b.inBounds(wx, wy) || !b.inBounds(bx, by)) return false;
                if (!isEnemySquare(b, p, wx, wy)) return false;

                // Épaisseur dans l'axe : exactement 1 case (la case derrière doit être non adverse).
                int thick = thicknessInAxis(b, p, wx, wy, -dx, -dy);
                if (thick != 1) return false;
                if (formsElbow(b, p, wx, wy, dx, dy)) return false;

                // R53 : le mur doit toucher par un côté un carré/base allié (la case "derrière"
                // en est la candidate naturelle, mais n'importe quel côté du mur compte).
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
            // Distance de Chebyshev entre cases les plus proches = max des écarts par axe.
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

            // Cas normal : contact côté ou coin avec un allié (R19).
            if (touchesAllyAny(b, p, pos.x, pos.y)) return true;

            // Cas traversée (R53) : de l'autre côté d'un mur adverse d'1 case.
            if (canCrossInto(b, p, pos)) return true;

            // Diagnostic : y a-t-il un mur adverse épais ou en coude juste à côté ?
            for (int d = 0; d < 4; ++d) {
                int wx = pos.x - DX4[d], wy = pos.y - DY4[d];
                if (b.inBounds(wx, wy) && isEnemySquare(b, p, wx, wy)) {
                    int thick = thicknessInAxis(b, p, wx, wy, -DX4[d], -DY4[d]);
                    if (thick >= 2 || formsElbow(b, p, wx, wy, DX4[d], DY4[d]))
                        return fail(Result::ThickWall);
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

        bool isBaseAccessible(const Board& b, Player target) {
            // T4 : on explore depuis toutes les sources de l'adversaire "attacker".
            Player attacker = opponent(target);

            const int W = b.width(), H = b.height();
            std::vector<char> seen(static_cast<size_t>(W) * H, 0);
            std::queue<Pos> q;

            // Sources : toutes les cases alliées (carré ou base) de l'attaquant.
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if (b.isOwnedBy(x, y, attacker)) {
                        seen[y * W + x] = 1;
                        q.push(Pos{x, y});
                    }

            auto touchesTarget = [&](int x, int y) {
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx, ny = y + dy;
                        if (b.inBounds(nx, ny) && b.isBaseOf(nx, ny, target)) return true;
                    }
                return false;
            };

            while (!q.empty()) {
                Pos cur = q.front();
                q.pop();

                // Cases voisines (côté ou coin) : on peut y poser un carré si elle est vide.
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = cur.x + dx, ny = cur.y + dy;
                        if (!b.inBounds(nx, ny)) continue;
                        if (seen[ny * W + nx]) continue;
                        if (!b.isEmpty(nx, ny)) continue;
                        seen[ny * W + nx] = 1;
                        if (touchesTarget(nx, ny)) return true;
                        q.push(Pos{nx, ny});
                    }

                // Traversée d'un mur adverse d'1 case depuis la case courante.
                for (int d = 0; d < 4; ++d) {
                    int wx = cur.x + DX4[d], wy = cur.y + DY4[d];
                    int tx = wx + DX4[d], ty = wy + DY4[d];
                    if (!b.inBounds(wx, wy) || !b.inBounds(tx, ty)) continue;
                    if (seen[ty * W + tx]) continue;
                    if (!b.isEmpty(tx, ty)) continue;
                    // Le mur doit être un carré de la cible, d'épaisseur 1, sans coude.
                    if (b.get(wx, wy) != squareOf(target)) continue;
                    if (thicknessInAxis(b, attacker, wx, wy, DX4[d], DY4[d]) != 1) continue;
                    if (formsElbow(b, attacker, wx, wy, DX4[d], DY4[d])) continue;
                    seen[ty * W + tx] = 1;
                    if (touchesTarget(tx, ty)) return true;
                    q.push(Pos{tx, ty});
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

                    // Simulation : on déplace la base sur une copie, puis on teste l'accessibilité.
                    Board copy = b;
                    for (int dy = 0; dy < 2; ++dy)
                        for (int dx = 0; dx < 2; ++dx) {
                            copy.set(oldBase.x + dx, oldBase.y + dy, Cell::Empty);
                            copy.set(x + dx, y + dy, baseOf(p));
                        }
                    // Les cases de l'ancienne base sont maintenant vides ; on remet les nouvelles.
                    // (Ordre : effacer puis écrire, donc si chevauchement la nouvelle gagne.)
                    for (int dy = 0; dy < 2; ++dy)
                        for (int dx = 0; dx < 2; ++dx)
                            copy.set(x + dx, y + dy, baseOf(p));

                    if (isBaseAccessible(copy, p)) result.push_back(tl);
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