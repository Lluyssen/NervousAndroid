#include <cstdio>
#include <cstdlib>
#include "Game.h"

using namespace ng;

static int g_failures = 0;
static int g_tests = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_tests;                                                           \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::printf("  ECHEC ligne %d : %s\n", __LINE__, #cond);         \
        }                                                                    \
    } while (0)

#define TEST(name) static void name()
#define RUN(name)                                  \
    do {                                           \
        std::printf("- %s\n", #name);              \
        name();                                    \
    } while (0)

// Crée une partie 20x20 avec J1 en (2,9) et J2 en (17,9) -> écart de 13 cases.
static Game makeGame() {
    Game g(20, 20);
    CHECK(g.placeBase(Pos{2, 9}) == Result::Ok);
    CHECK(g.placeBase(Pos{17, 9}) == Result::Ok);
    return g;
}

// ------------------------------------------------------------------ Mise en place

TEST(grille_taille_limites) {
    bool threw = false;
    try { Game g(14, 20); } catch (...) { threw = true; }
    CHECK(threw);
    threw = false;
    try { Game g(20, 101); } catch (...) { threw = true; }
    CHECK(threw);
    threw = false;
    try { Game g(15, 100); } catch (...) { threw = true; }
    CHECK(!threw);
}

TEST(base_bord_autorise) {
    Game g(20, 20);
    CHECK(g.placeBase(Pos{0, 0}) == Result::Ok);   // coin de la grille
}

TEST(base_hors_grille_refusee) {
    Game g(20, 20);
    CHECK(g.placeBase(Pos{19, 19}) == Result::BaseOverlap);  // 2x2 déborde
}

TEST(base_distance_minimale_5) {
    Game g(30, 30);
    CHECK(g.placeBase(Pos{5, 5}) == Result::Ok);   // occupe x 5-6, y 5-6
    // Écart de 4 cases vides (x 7..10) -> refusé. Base J2 en x=11.
    CHECK(g.placeBase(Pos{11, 5}) == Result::BaseTooClose);
    // Écart de 5 cases vides (x 7..11) -> accepté. Base J2 en x=12.
    CHECK(g.placeBase(Pos{12, 5}) == Result::Ok);
}

TEST(base_distance_diagonale) {
    Game g(30, 30);
    CHECK(g.placeBase(Pos{5, 5}) == Result::Ok);
    // Écart 5 en x et 5 en y : Chebyshev = 5 -> accepté.
    CHECK(g.placeBase(Pos{12, 12}) == Result::Ok);
}

TEST(base_chevauchement_refuse) {
    Game g(30, 30);
    CHECK(g.placeBase(Pos{5, 5}) == Result::Ok);
    CHECK(g.placeBase(Pos{6, 6}) != Result::Ok);
}

TEST(j1_joue_en_premier) {
    Game g = makeGame();
    CHECK(g.phase() == Phase::PlaySquares);
    CHECK(g.current() == Player::One);
    CHECK(g.hp(Player::One) == 4);
    CHECK(g.hp(Player::Two) == 4);
}

// ------------------------------------------------------------------ Pose

TEST(pose_adjacente_cote_et_coin) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);    // côté de la base (x 2-3)
    CHECK(g.placeSquare(Pos{5, 10}) == Result::Ok);   // coin du carré précédent
}

TEST(pose_non_connectee_refusee) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{8, 9}) == Result::NotConnected);
}

TEST(pose_case_occupee_refusee) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{4, 9}) == Result::OccupiedCell);
    CHECK(g.placeSquare(Pos{2, 9}) == Result::OccupiedCell);   // dans sa propre base
}

TEST(pose_hors_grille_refusee) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{-1, 0}) == Result::OutOfBounds);
    CHECK(g.placeSquare(Pos{20, 0}) == Result::OutOfBounds);
}

TEST(deuxieme_carre_touche_le_premier) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);    // touche le 1er carré du même tour
}

TEST(max_deux_poses_et_passage_automatique) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    CHECK(g.current() == Player::Two);                // tour passé automatiquement
    CHECK(g.placedThisTurn() == 0);
}

TEST(fin_de_tour_sans_pose_interdite) {
    Game g = makeGame();
    CHECK(g.endTurn() == Result::NeedAtLeastOne);     // R49
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);                 // 1 pose suffit
    CHECK(g.current() == Player::Two);
}

int main() {
    RUN(grille_taille_limites);
    RUN(base_bord_autorise);
    RUN(base_hors_grille_refusee);
    RUN(base_distance_minimale_5);
    RUN(base_distance_diagonale);
    RUN(base_chevauchement_refuse);
    RUN(j1_joue_en_premier);
    RUN(pose_adjacente_cote_et_coin);
    RUN(pose_non_connectee_refusee);
    RUN(pose_case_occupee_refusee);
    RUN(pose_hors_grille_refusee);
    RUN(deuxieme_carre_touche_le_premier);
    RUN(max_deux_poses_et_passage_automatique);
    RUN(fin_de_tour_sans_pose_interdite);

    std::printf("\n%d verifications, %d echec(s)\n", g_tests, g_failures);
    return g_failures == 0 ? 0 : 1;
}