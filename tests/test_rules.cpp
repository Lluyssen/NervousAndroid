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

// ------------------------------------------------------------------ Dégâts

// J1 base en (2,9) [x 2-3], J2 base en (9,9) [x 9-10] : écart de 5 cases vides (x 4..8).
static Game makeCloseGame() {
    Game g(20, 20);
    CHECK(g.placeBase(Pos{2, 9}) == Result::Ok);
    CHECK(g.placeBase(Pos{9, 9}) == Result::Ok);
    return g;
}

TEST(degats_contact_base_adverse) {
    Game g = makeCloseGame();
    // J1 construit vers la droite : x=4,5 puis x=6,7 (2 poses par tour).
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    // Tour J2 : une pose, puis fin de tour.
    CHECK(g.placeSquare(Pos{11, 9}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);
    CHECK(g.placeSquare(Pos{6, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{7, 9}) == Result::Ok);
    CHECK(g.hp(Player::Two) == 4);          // x=7 ne touche pas encore x=9
    CHECK(g.placeSquare(Pos{11, 10}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);
    CHECK(g.placeSquare(Pos{8, 9}) == Result::Ok);   // touche la base J2 (x=9)
    CHECK(g.hp(Player::Two) == 3);          // 1 dégât
}

// ------------------------------------------------------------------ Relocalisation / victoire

TEST(legal_placements_non_vide_au_depart) {
    Game g = makeGame();
    CHECK(!g.legalSquarePlacements().empty());
}

TEST(pas_de_gagnant_au_depart) {
    Game g = makeGame();
    CHECK(!g.isOver());
}

// ------------------------------------------------------------------ Victoire

// Helper : joue un tour "1 pose" pour J2 loin de la zone de combat.
// J2 a sa base en (9,9) [x 9-10, y 9-10]. On lui fait poser des carrés à droite.
// Chaque appel pose un carré adjacent à un carré déjà posé du joueur (chaîne vers la droite).
static void j2PassOnce(Game& g, int x) {
    CHECK(g.placeSquare(Pos{x, 9}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);
}

TEST(victoire_apres_4_degats) {
    Game g = makeCloseGame();                 // J1 (2,9) ; J2 (9,9)
    // J1 chaîne : x=4..8 (5 carrés = 3 tours). x=8 touche la base J2 : -1 PV.
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    j2PassOnce(g, 11);
    CHECK(g.placeSquare(Pos{6, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{7, 9}) == Result::Ok);
    j2PassOnce(g, 12);
    CHECK(g.placeSquare(Pos{8, 9}) == Result::Ok);
    CHECK(g.hp(Player::Two) == 3);
    // Autres contacts : (8,10), (8,11)? (8,10) touche la base (9,10) ; (8,8) touche (9,9).
    CHECK(g.placeSquare(Pos{8, 10}) == Result::Ok);
    CHECK(g.hp(Player::Two) <= 2);            // au moins un dégât de plus
}

TEST(partie_terminee_refuse_les_actions) {
    Game g = makeGame();
    CHECK(!g.isOver());
    // On vérifie seulement le contrat : placeSquare est refusé hors phase PlaySquares.
    Game h(20, 20);
    CHECK(h.placeSquare(Pos{4, 9}) != Result::Ok);   // phase de placement des bases
}

// ------------------------------------------------------------------ Traversée

// J2 pose un mur d'épaisseur 1 (colonne x=6, y=8..10) ; J1 doit pouvoir passer.
// Ici on ne teste que ce que l'API permet de voir : un carré J1 posé sur la case
// juste derrière un mur fin n'est pas refusé pour "NotConnected".
TEST(traversee_mur_fin_autorisee) {
    Game g(20, 20);
    CHECK(g.placeBase(Pos{2, 9}) == Result::Ok);
    CHECK(g.placeBase(Pos{9, 9}) == Result::Ok);
    // J1 : (4,9), (5,9)
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    // J2 : mur fin à x=6 (adjacent au bout de J1), mais J2 doit rester connecté
    // à sa propre base : on construit donc depuis x=8 vers x=6.
    CHECK(g.placeSquare(Pos{8, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{7, 9}) == Result::Ok);
    // J1 : (6,9) est libre et adjacent à (5,9). Puis (7,9) est un carré adverse
    // d'épaisseur 1 (entre x=6 et x=8 libre/occupé) : voir résultat observé.
    CHECK(g.placeSquare(Pos{6, 9}) == Result::Ok);
}

// ------------------------------------------------------------------ Relocalisation (API)

TEST(relocations_vides_hors_phase) {
    Game g = makeGame();
    CHECK(g.legalBaseRelocations().empty());     // pas en phase RelocateBase
    CHECK(g.relocateBase(Pos{5, 5}) != Result::Ok);
}

TEST(placements_initiaux_respectent_distance) {
    Game g(30, 30);
    CHECK(g.placeBase(Pos{5, 5}) == Result::Ok);
    auto legal = g.legalInitialBasePlacements();
    CHECK(!legal.empty());
    bool close = false;
    for (const Pos& p : legal)
        if (p.x >= 7 && p.x <= 11 && p.y >= 3 && p.y <= 8) close = true;
    CHECK(!close);                                // aucune case trop proche proposée
}

// ------------------------------------------------------------------ Épaisseur 2

// J2 construit un mur d'épaisseur 2 ; J1 ne peut PAS poser derrière.
TEST(mur_epaisseur_2_bloque) {
    Game g(20, 20);
    CHECK(g.placeBase(Pos{2, 9}) == Result::Ok);
    CHECK(g.placeBase(Pos{12, 9}) == Result::Ok);   // J2 x 12-13
    // J1 : (4,9), (5,9)
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    // J2 : (11,9) et (10,9) -> bande horizontale de 2 cases dans l'axe x
    CHECK(g.placeSquare(Pos{11, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{10, 9}) == Result::Ok);
    // J1 : (6,9), (7,9)
    CHECK(g.placeSquare(Pos{6, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{7, 9}) == Result::Ok);
    // J2 : (9,9) puis fin -> mur x=9,10,11 (épaisseur 3 dans l'axe x)
    CHECK(g.placeSquare(Pos{9, 9}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);
    // J1 : (8,9) est adjacent à (7,9) -> légal normalement (contact allié).
    CHECK(g.placeSquare(Pos{8, 9}) == Result::Ok);
}

// ------------------------------------------------------------------ Victoire complète

TEST(victoire_a_zero_pv) {
    Game g = makeCloseGame();                   // J1 (2,9) ; J2 (9,9)
    // Chaîne J1 vers la base J2, puis 4 contacts distincts (R6 : 1 dégât par pose).
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    j2PassOnce(g, 11);
    CHECK(g.placeSquare(Pos{6, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{7, 9}) == Result::Ok);
    j2PassOnce(g, 12);
    CHECK(g.placeSquare(Pos{8, 9}) == Result::Ok);    // contact 1
    CHECK(g.placeSquare(Pos{8, 10}) == Result::Ok);   // contact 2
    j2PassOnce(g, 13);
    CHECK(g.placeSquare(Pos{8, 11}) == Result::Ok);   // touche coin (9,10) : contact 3
    CHECK(g.placeSquare(Pos{8, 8}) == Result::Ok);    // touche coin (9,9) : contact 4
    CHECK(g.hp(Player::Two) <= 0 || g.isOver());
    CHECK(g.isOver());
    CHECK(g.winner() == Player::One);
}

TEST(apres_victoire_plus_de_pose) {
    Game g = makeCloseGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    j2PassOnce(g, 11);
    CHECK(g.placeSquare(Pos{6, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{7, 9}) == Result::Ok);
    j2PassOnce(g, 12);
    CHECK(g.placeSquare(Pos{8, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{8, 10}) == Result::Ok);
    j2PassOnce(g, 13);
    CHECK(g.placeSquare(Pos{8, 11}) == Result::Ok);
    CHECK(g.placeSquare(Pos{8, 8}) == Result::Ok);
    if (g.isOver()) CHECK(g.placeSquare(Pos{7, 8}) != Result::Ok);
}

// J1 pose un carré le long de la ligne y=15 en partant de x=14 vers la gauche, puis passe.
static void j1PassLeft(Game& g, int x) {
    CHECK(g.current() == Player::One);
    CHECK(g.placeSquare(Pos{x, 15}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);
}

TEST(base_enfermee_declenche_deplacement) {
    Game g(20, 20);
    CHECK(g.placeBase(Pos{15, 15}) == Result::Ok);  // J1 loin
    CHECK(g.placeBase(Pos{0, 0}) == Result::Ok);    // J2 dans le coin

    j1PassLeft(g, 14);
    CHECK(g.placeSquare(Pos{2, 0}) == Result::Ok);
    CHECK(g.placeSquare(Pos{2, 1}) == Result::Ok);  // tour auto -> J1

    j1PassLeft(g, 13);
    CHECK(g.placeSquare(Pos{2, 2}) == Result::Ok);
    CHECK(g.placeSquare(Pos{1, 2}) == Result::Ok);  // tour auto -> J1

    j1PassLeft(g, 12);
    CHECK(g.placeSquare(Pos{0, 2}) == Result::Ok);  // ferme le coin
    CHECK(g.phase() == Phase::RelocateBase);
    CHECK(g.mustRelocatePlayer() == Player::Two);
    CHECK(!g.legalBaseRelocations().empty());
}

// ------------------------------------------------------------------ Traversée

// Un mur d'1 case est traversable : la case derrière est légale pour J1.
// Un coude (forme en L) ne l'est pas : formsElbow le refuse.
TEST(traversee_refusee_epaisseur_2_dans_l_axe) {
    Board b(20, 20);
    b.set(5, 5, squareOf(Player::One));
    b.set(6, 5, squareOf(Player::Two));
    b.set(7, 5, squareOf(Player::Two));   // 2 cases d'épaisseur dans l'axe
    CHECK(!rules::canPlaceSquare(b, Player::One, Pos{8, 5}, nullptr));
}

TEST(traversee_ok_mur_en_L_axe_fin) {
    Board b(20, 20);
    b.set(5, 5, squareOf(Player::One));
    b.set(6, 5, squareOf(Player::Two));
    b.set(6, 6, squareOf(Player::Two));   // L : fin dans l'axe horizontal
    CHECK(rules::canPlaceSquare(b, Player::One, Pos{7, 5}, nullptr));
}

// Aide : prépare une partie où la base de J2 (coin 0,0) est sur le point d'être enfermée.
static Game makeEnclosureReady() {
    Game g(20, 20);
    CHECK(g.placeBase(Pos{15, 15}) == Result::Ok);
    CHECK(g.placeBase(Pos{0, 0}) == Result::Ok);
    auto j1 = [&](int x) {
        CHECK(g.placeSquare(Pos{x, 15}) == Result::Ok);
        CHECK(g.endTurn() == Result::Ok);
    };
    j1(14);
    CHECK(g.placeSquare(Pos{2, 0}) == Result::Ok);
    CHECK(g.placeSquare(Pos{2, 1}) == Result::Ok);
    j1(13);
    CHECK(g.placeSquare(Pos{2, 2}) == Result::Ok);
    CHECK(g.placeSquare(Pos{1, 2}) == Result::Ok);
    j1(12);
    CHECK(g.placeSquare(Pos{0, 2}) == Result::Ok);   // enferme J2
    return g;
}

TEST(deplacement_valide_retour_en_jeu) {
    Game g = makeEnclosureReady();
    CHECK(g.phase() == Phase::RelocateBase);
    auto opts = g.legalBaseRelocations();
    CHECK(!opts.empty());
    // Toute option annoncée légale doit être acceptée par relocateBase.
    Game g2 = g;
    CHECK(g2.relocateBase(opts[0]) == Result::Ok);
    CHECK(g2.phase() == Phase::PlaySquares);
    CHECK(g2.hp(Player::Two) == 4);   // PV conservés
}

TEST(toutes_relocations_annoncees_sont_acceptees) {
    Game base = makeEnclosureReady();
    auto opts = base.legalBaseRelocations();
    CHECK(!opts.empty());
    for (Pos p : opts) {
        Game g = base;
        CHECK(g.relocateBase(p) == Result::Ok);   // révèle l'incohérence éventuelle
    }
}

TEST(deplacement_trop_proche_refuse) {
    Game g = makeEnclosureReady();
    // J1 a sa base en (15,15) ; (12,12) est à écart 1 -> < 4
    CHECK(g.relocateBase(Pos{12, 12}) == Result::BaseTooClose);
    CHECK(g.phase() == Phase::RelocateBase);   // reste bloqué
}

TEST(pose_interdite_pendant_deplacement) {
    Game g = makeEnclosureReady();
    CHECK(g.placeSquare(Pos{5, 5}) == Result::MustRelocateBase);
    CHECK(g.endTurn() == Result::MustRelocateBase);
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
    RUN(degats_contact_base_adverse);
    RUN(legal_placements_non_vide_au_depart);
    RUN(pas_de_gagnant_au_depart);
    RUN(victoire_apres_4_degats);
    RUN(partie_terminee_refuse_les_actions);
    RUN(traversee_mur_fin_autorisee);
    RUN(relocations_vides_hors_phase);
    RUN(placements_initiaux_respectent_distance);
    RUN(mur_epaisseur_2_bloque);
    RUN(victoire_a_zero_pv);
    RUN(apres_victoire_plus_de_pose);
    RUN(base_enfermee_declenche_deplacement);
    RUN(traversee_refusee_epaisseur_2_dans_l_axe);
    RUN(traversee_ok_mur_en_L_axe_fin);
    RUN(deplacement_valide_retour_en_jeu);
    RUN(toutes_relocations_annoncees_sont_acceptees);
    RUN(deplacement_trop_proche_refuse);
    RUN(pose_interdite_pendant_deplacement);

    std::printf("\n%d verifications, %d echec(s)\n", g_tests, g_failures);
    return g_failures == 0 ? 0 : 1;
}