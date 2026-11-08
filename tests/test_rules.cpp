#include <cstdio>
#include <cstdlib>
#include <iostream>
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

// Pose les deux bases 2x2 de test sur un Board brut : J2 en (j2x,j2y), J1 en (j1x,j1y).
static void putBases(Board& b, int j2x, int j2y, int j1x, int j1y) {
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
            b.set(j2x + dx, j2y + dy, baseOf(Player::Two));
            b.set(j1x + dx, j1y + dy, baseOf(Player::One));
        }
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
    CHECK(g.placeBase(Pos{0, 0}) == Result::Ok);
}

TEST(base_hors_grille_refusee) {
    Game g(20, 20);
    CHECK(g.placeBase(Pos{19, 19}) == Result::BaseOverlap);
}

TEST(base_distance_minimale_5) {
    Game g(30, 30);
    CHECK(g.placeBase(Pos{5, 5}) == Result::Ok);
    CHECK(g.placeBase(Pos{11, 5}) == Result::BaseTooClose);
    CHECK(g.placeBase(Pos{12, 5}) == Result::Ok);
}

TEST(base_distance_diagonale) {
    Game g(30, 30);
    CHECK(g.placeBase(Pos{5, 5}) == Result::Ok);
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

TEST(placement_refuse_pendant_mise_en_place) {
    Game h(20, 20);
    CHECK(h.placeSquare(Pos{4, 9}) == Result::WrongPhase);
    CHECK(h.endTurn() == Result::WrongPhase);
}

TEST(placement_base_refuse_en_jeu) {
    Game g = makeGame();
    CHECK(g.placeBase(Pos{8, 3}) == Result::WrongPhase);
}

TEST(actions_refusees_pendant_placement_bases) {
    Game h(20, 20);
    CHECK(h.placeBase(Pos{2, 9}) == Result::Ok);
    CHECK(h.placeSquare(Pos{4, 9}) == Result::WrongPhase);
}

// ------------------------------------------------------------------ Pose


TEST(pose_adjacente_par_un_cote) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);    // touche la base (3,9) par un cÃītÃĐ
    CHECK(g.placeSquare(Pos{4, 10}) == Result::Ok);   // touche la base (3,10) et (4,9) par un cÃītÃĐ
}

TEST(pose_en_coin_seul_refusee) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 10}) == Result::NotConnected);   // coin seulement
    CHECK(g.placedThisTurn() == 1);                              // la pose refusÃĐe ne compte pas
}

TEST(pose_non_connectee_refusee) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{8, 9}) == Result::NotConnected);
}

TEST(pose_case_occupee_refusee) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{4, 9}) == Result::OccupiedCell);
    CHECK(g.placeSquare(Pos{2, 9}) == Result::OccupiedCell);
}

TEST(pose_hors_grille_refusee) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{-1, 0}) == Result::OutOfBounds);
    CHECK(g.placeSquare(Pos{20, 0}) == Result::OutOfBounds);
}

TEST(deuxieme_carre_touche_le_premier) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
}

TEST(max_deux_poses_et_passage_automatique) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    CHECK(g.current() == Player::Two);
    CHECK(g.placedThisTurn() == 0);
}

TEST(troisieme_pose_refusee_dans_le_tour) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    // Le tour a basculé : on est à J2, donc J1 ne peut plus jouer ici.
    CHECK(g.current() == Player::Two);
}

TEST(compteur_poses_du_tour) {
    Game g = makeGame();
    CHECK(g.placedThisTurn() == 0);
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placedThisTurn() == 1);
}

TEST(pose_refusee_ne_consomme_pas) {
    Game g = makeGame();
    CHECK(g.placeSquare(Pos{8, 9}) == Result::NotConnected);
    CHECK(g.placedThisTurn() == 0);
    CHECK(g.current() == Player::One);
}

TEST(fin_de_tour_sans_pose_interdite) {
    Game g = makeGame();
    CHECK(g.endTurn() == Result::NeedAtLeastOne);
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);
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
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{11, 9}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);
    CHECK(g.placeSquare(Pos{6, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{7, 9}) == Result::Ok);
    CHECK(g.hp(Player::Two) == 4);
    CHECK(g.placeSquare(Pos{11, 10}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);
    CHECK(g.placeSquare(Pos{8, 9}) == Result::Ok);
    CHECK(g.hp(Player::Two) == 3);
}

TEST(degats_ne_touchent_pas_l_attaquant) {
    Game g = makeCloseGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    CHECK(g.hp(Player::One) == 4);
    CHECK(g.hp(Player::Two) == 4);
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

// Helper : joue un tour "1 pose" pour J2 loin de la zone de combat.
static void j2PassOnce(Game& g, int x) {
    CHECK(g.placeSquare(Pos{x, 9}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);
}

TEST(victoire_apres_4_degats) {
    Game g = makeCloseGame();
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    j2PassOnce(g, 11);
    CHECK(g.placeSquare(Pos{6, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{7, 9}) == Result::Ok);
    j2PassOnce(g, 12);
    CHECK(g.placeSquare(Pos{8, 9}) == Result::Ok);
    CHECK(g.hp(Player::Two) == 3);
    CHECK(g.placeSquare(Pos{8, 10}) == Result::Ok);
    CHECK(g.hp(Player::Two) <= 2);
}

// ------------------------------------------------------------------ Traversée

TEST(traversee_mur_fin_avance) {
    Board b(20, 20);
    putBases(b, 12, 9, 2, 9);
    b.set(5, 9, squareOf(Player::One));
    b.set(6, 9, squareOf(Player::Two));
    b.set(6, 10, squareOf(Player::Two));
    CHECK(rules::canPlaceSquare(b, Player::One, Pos{7, 9}, nullptr));
}

TEST(traversee_mur_epais_bloque_case_suivante) {
    Board b(20, 20);
    putBases(b, 12, 9, 2, 9);
    b.set(5, 9, squareOf(Player::One));
    b.set(6, 9, squareOf(Player::Two));
    b.set(7, 9, squareOf(Player::Two));
    CHECK(!rules::canPlaceSquare(b, Player::One, Pos{8, 9}, nullptr));
}

TEST(relocations_vides_hors_phase) {
    Game g = makeGame();
    CHECK(g.legalBaseRelocations().empty());
    CHECK(g.relocateBase(Pos{5, 5}) == Result::WrongPhase);
}

TEST(placements_initiaux_respectent_distance) {
    Game g(30, 30);
    CHECK(g.placeBase(Pos{5, 5}) == Result::Ok);
    auto legal = g.legalInitialBasePlacements();
    CHECK(!legal.empty());
    bool close = false;
    for (const Pos& p : legal)
        if (p.x >= 7 && p.x <= 11 && p.y >= 3 && p.y <= 8) close = true;
    CHECK(!close);
}

// ------------------------------------------------------------------ Épaisseur 2

TEST(mur_epaisseur_2_bloque) {
    Game g(20, 20);
    CHECK(g.placeBase(Pos{2, 9}) == Result::Ok);
    CHECK(g.placeBase(Pos{12, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{4, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{5, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{11, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{10, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{6, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{7, 9}) == Result::Ok);
    CHECK(g.placeSquare(Pos{9, 9}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);
    CHECK(g.placeSquare(Pos{8, 9}) == Result::Ok);
    CHECK(!rules::canPlaceSquare(g.board(), Player::One, Pos{14, 9}, nullptr));
}

// ------------------------------------------------------------------ Victoire complète

TEST(victoire_a_zero_pv) {
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
    CHECK(g.isOver());
    CHECK(g.placeSquare(Pos{7, 8}) == Result::GameOver);
    CHECK(g.endTurn() == Result::GameOver);
}

// J1 pose un carré le long de la ligne y=15 en partant de x=14 vers la gauche, puis passe.
static void j1PassLeft(Game& g, int x) {
    CHECK(g.current() == Player::One);
    CHECK(g.placeSquare(Pos{x, 15}) == Result::Ok);
    CHECK(g.endTurn() == Result::Ok);
}

TEST(base_enfermee_declenche_deplacement) {
    Game g(20, 20);
    CHECK(g.placeBase(Pos{15, 15}) == Result::Ok);
    CHECK(g.placeBase(Pos{0, 0}) == Result::Ok);

    j1PassLeft(g, 14);
    CHECK(g.placeSquare(Pos{2, 0}) == Result::Ok);
    CHECK(g.placeSquare(Pos{2, 1}) == Result::Ok);

    j1PassLeft(g, 13);
    CHECK(g.placeSquare(Pos{2, 2}) == Result::Ok);
    CHECK(g.placeSquare(Pos{1, 2}) == Result::Ok);

    j1PassLeft(g, 12);
    CHECK(g.placeSquare(Pos{0, 2}) == Result::Ok);
    CHECK(g.phase() == Phase::RelocateBase);
    CHECK(g.mustRelocatePlayer() == Player::Two);
    CHECK(!g.legalBaseRelocations().empty());
}

// ------------------------------------------------------------------ Traversée (règles)

TEST(traversee_refusee_epaisseur_2_dans_l_axe) {
    Board b(20, 20);
    b.set(5, 5, squareOf(Player::One));
    b.set(6, 5, squareOf(Player::Two));
    b.set(7, 5, squareOf(Player::Two));
    CHECK(!rules::canPlaceSquare(b, Player::One, Pos{8, 5}, nullptr));
}

TEST(traversee_ok_mur_en_L_axe_fin) {
    Board b(20, 20);
    b.set(5, 5, squareOf(Player::One));
    b.set(6, 5, squareOf(Player::Two));
    b.set(6, 6, squareOf(Player::Two));
    CHECK(rules::canPlaceSquare(b, Player::One, Pos{7, 5}, nullptr));
}

// Aide : prépare une partie où la base de J2 (coin 0,0) est enfermée.
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
    CHECK(g.placeSquare(Pos{0, 2}) == Result::Ok);
    return g;
}

TEST(deplacement_valide_retour_en_jeu) {
    Game g = makeEnclosureReady();
    CHECK(g.phase() == Phase::RelocateBase);
    auto opts = g.legalBaseRelocations();
    CHECK(!opts.empty());
    Game g2 = g;
    CHECK(g2.relocateBase(opts[0]) == Result::Ok);
    CHECK(g2.phase() == Phase::PlaySquares);
    CHECK(g2.hp(Player::Two) == 4);
}

TEST(toutes_relocations_annoncees_sont_acceptees) {
    Game base = makeEnclosureReady();
    auto opts = base.legalBaseRelocations();
    CHECK(!opts.empty());
    for (Pos p : opts) {
        Game g = base;
        CHECK(g.relocateBase(p) == Result::Ok);
    }
}

TEST(deplacement_trop_proche_refuse) {
    Game g = makeEnclosureReady();
    CHECK(g.relocateBase(Pos{12, 12}) == Result::BaseTooClose);
    CHECK(g.phase() == Phase::RelocateBase);
}

TEST(deplacement_sur_case_occupee_refuse) {
    Game g = makeEnclosureReady();
    CHECK(g.relocateBase(Pos{14, 14}) == Result::BaseOverlap);
    CHECK(g.phase() == Phase::RelocateBase);
}

TEST(deplacement_hors_grille_refuse) {
    Game g = makeEnclosureReady();
    CHECK(g.relocateBase(Pos{19, 19}) == Result::BaseOverlap);
    CHECK(g.phase() == Phase::RelocateBase);
}

TEST(pose_interdite_pendant_deplacement) {
    Game g = makeEnclosureReady();
    CHECK(g.placeSquare(Pos{5, 5}) == Result::MustRelocateBase);
    CHECK(g.endTurn() == Result::MustRelocateBase);
}

// ------------------------------------------------------------------ Enfermement et traversée (règles)

TEST(nouveau_bases_libres_non_enfermees) {
    Board b(20, 20);
    putBases(b, 15, 9, 2, 9);
    CHECK(rules::isBaseAccessible(b, Player::One));
    CHECK(rules::isBaseAccessible(b, Player::Two));
    CHECK(!rules::isEnclosed(b, Player::One));
    CHECK(!rules::isEnclosed(b, Player::Two));
}

TEST(nouveau_mur_epaisseur_1_base_accessible) {
    Board b(20, 20);
    putBases(b, 0, 0, 15, 15);
    for (int y = 0; y <= 3; ++y) b.set(3, y, squareOf(Player::Two));
    for (int x = 0; x <= 3; ++x) b.set(x, 3, squareOf(Player::Two));
    b.set(4, 0, squareOf(Player::One));
    b.set(4, 1, squareOf(Player::One));
    CHECK(rules::isBaseAccessible(b, Player::Two));
    CHECK(!rules::isEnclosed(b, Player::Two));
}

TEST(nouveau_mur_epaisseur_2_base_enfermee) {
    Board b(20, 20);
    putBases(b, 0, 0, 15, 15);
    for (int y = 0; y <= 3; ++y) {
        b.set(2, y, squareOf(Player::Two));
        b.set(3, y, squareOf(Player::Two));
    }
    for (int x = 0; x <= 1; ++x) {
        b.set(x, 2, squareOf(Player::Two));
        b.set(x, 3, squareOf(Player::Two));
    }
    for (int k = 0; k <= 3; ++k) {
        b.set(4, k, squareOf(Player::One));
        b.set(k, 4, squareOf(Player::One));
    }
    CHECK(!rules::isBaseAccessible(b, Player::Two));
    CHECK(rules::isEnclosed(b, Player::Two));
}

TEST(nouveau_contact_direct_base_accessible) {
    Board b(20, 20);
    putBases(b, 5, 5, 15, 15);
    b.set(7, 5, squareOf(Player::One));
    CHECK(rules::isBaseAccessible(b, Player::Two));
}

TEST(nouveau_contact_diagonal_base_accessible) {
    Board b(20, 20);
    putBases(b, 5, 5, 15, 15);
    b.set(7, 7, squareOf(Player::One));
    CHECK(rules::isBaseAccessible(b, Player::Two));
}

TEST(nouveau_apres_deplacement_plus_enfermee) {
    Game g = makeEnclosureReady();
    CHECK(g.phase() == Phase::RelocateBase);
    auto opts = g.legalBaseRelocations();
    CHECK(!opts.empty());
    CHECK(g.relocateBase(opts[0]) == Result::Ok);
    CHECK(g.phase() == Phase::PlaySquares);
    CHECK(!rules::isEnclosed(g.board(), Player::Two));
}

TEST(poche_vide_fermee_non_accessible) {
    Board b(20, 20);
    putBases(b, 0, 0, 15, 15);
    for (int k = 0; k <= 4; ++k) {
        b.set(3, k, squareOf(Player::Two));
        b.set(4, k, squareOf(Player::Two));
        b.set(k, 3, squareOf(Player::Two));
        b.set(k, 4, squareOf(Player::Two));
    }
    CHECK(!rules::isBaseAccessible(b, Player::Two));
    CHECK(rules::isEnclosed(b, Player::Two));
}

TEST(nouveau_poche_mur_fin_accessible_par_traversee) {
    Board b(20, 20);
    putBases(b, 0, 0, 15, 15);
    for (int k = 0; k <= 3; ++k) {
        b.set(3, k, squareOf(Player::Two));
        b.set(k, 3, squareOf(Player::Two));
    }
    b.set(4, 1, squareOf(Player::One));
    CHECK(rules::isBaseAccessible(b, Player::Two));
}

TEST(nouveau_attaquant_deconnecte_inaccessible) {
    Board b(20, 20);
    putBases(b, 0, 0, 15, 15);
    for (int k = 0; k <= 4; ++k) {
        b.set(3, k, squareOf(Player::Two));
        b.set(4, k, squareOf(Player::Two));
        b.set(k, 3, squareOf(Player::Two));
        b.set(k, 4, squareOf(Player::Two));
    }
    CHECK(!rules::isBaseAccessible(b, Player::Two));
}

TEST(nouveau_j1_enferme_symetrique) {
    Board b(20, 20);
    putBases(b, 15, 15, 0, 0);

    // mur de J1 en L, epaisseur 2, ferme dans le coin
    // mur vertical : colonnes 3-4, de y=0 a y=4
    for (int y = 0; y <= 4; ++y) {
        b.set(3, y, squareOf(Player::One));
        b.set(4, y, squareOf(Player::One));
    }
    // mur horizontal : lignes 3-4, de x=0 a x=4
    for (int x = 0; x <= 4; ++x) {
        b.set(x, 3, squareOf(Player::One));
        b.set(x, 4, squareOf(Player::One));
    }

    // carres d'attaque de J2 colles au mur
    for (int k = 0; k <= 3; ++k) {
        b.set(5, k, squareOf(Player::Two));
        b.set(k, 5, squareOf(Player::Two));
    }

    CHECK(!rules::isBaseAccessible(b, Player::One));
    CHECK(rules::isEnclosed(b, Player::One));
}

TEST(nouveau_base_coin_grille_sans_debordement) {
    Board b(20, 20);
    putBases(b, 0, 0, 18, 18);
    CHECK(rules::isBaseAccessible(b, Player::Two));
    CHECK(!rules::isEnclosed(b, Player::Two));
}

TEST(ancienne_base_devient_carres) {
    Game g = makeEnclosureReady();
    CHECK(g.phase() == Phase::RelocateBase);
    auto opts = g.legalBaseRelocations();
    CHECK(!opts.empty());
    CHECK(g.relocateBase(opts[0]) == Result::Ok);
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx)
            CHECK(g.board().get(0 + dx, 0 + dy) == squareOf(Player::Two));
}

TEST(diagnostic_pourquoi_refuse) {
    Game g = makeEnclosureReady();
    for (int y = 0; y + 1 < 15; ++y) {
        for (int x = 0; x + 1 < 15; ++x) {
            Game t = g;
            Result r = t.relocateBase(Pos{x, y});
            std::printf("%c", r == Result::Ok ? '.' :
                              r == Result::BaseOverlap ? 'O' :
                              r == Result::BaseTooClose ? 'C' :
                              r == Result::BaseNotAccessible ? 'A' : '?');
        }
        std::printf("\n");
    }
}

// ------------------------------------------------------------------ Traversée

int main() {
    RUN(grille_taille_limites);
    RUN(base_bord_autorise);
    RUN(base_hors_grille_refusee);
    RUN(base_distance_minimale_5);
    RUN(base_distance_diagonale);
    RUN(base_chevauchement_refuse);
    RUN(j1_joue_en_premier);
    RUN(placement_refuse_pendant_mise_en_place);
    RUN(placement_base_refuse_en_jeu);
    RUN(actions_refusees_pendant_placement_bases);
    RUN(pose_adjacente_par_un_cote);
    RUN(pose_en_coin_seul_refusee);
    RUN(pose_non_connectee_refusee);
    RUN(pose_case_occupee_refusee);
    RUN(pose_hors_grille_refusee);
    RUN(deuxieme_carre_touche_le_premier);
    RUN(max_deux_poses_et_passage_automatique);
    RUN(troisieme_pose_refusee_dans_le_tour);
    RUN(compteur_poses_du_tour);
    RUN(pose_refusee_ne_consomme_pas);
    RUN(fin_de_tour_sans_pose_interdite);
    RUN(degats_contact_base_adverse);
    RUN(degats_ne_touchent_pas_l_attaquant);
    RUN(legal_placements_non_vide_au_depart);
    RUN(pas_de_gagnant_au_depart);
    RUN(victoire_apres_4_degats);
    RUN(traversee_mur_fin_avance);
    RUN(traversee_mur_epais_bloque_case_suivante);
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
    RUN(deplacement_sur_case_occupee_refuse);
    RUN(deplacement_hors_grille_refuse);
    RUN(pose_interdite_pendant_deplacement);

    RUN(nouveau_bases_libres_non_enfermees);
    RUN(nouveau_mur_epaisseur_1_base_accessible);
    RUN(nouveau_mur_epaisseur_2_base_enfermee);
    RUN(nouveau_contact_direct_base_accessible);
    RUN(nouveau_contact_diagonal_base_accessible);
    RUN(nouveau_apres_deplacement_plus_enfermee);
    RUN(poche_vide_fermee_non_accessible);
    RUN(nouveau_poche_mur_fin_accessible_par_traversee);
    RUN(nouveau_attaquant_deconnecte_inaccessible);
    RUN(nouveau_j1_enferme_symetrique);
    RUN(nouveau_base_coin_grille_sans_debordement);
    RUN(ancienne_base_devient_carres);

    RUN(diagnostic_pourquoi_refuse);

    std::printf("\n%d verifications, %d echec(s)\n", g_tests, g_failures);
    return g_failures == 0 ? 0 : 1;
}