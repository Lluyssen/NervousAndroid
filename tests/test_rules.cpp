// tests/test_game.cpp
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <string>
#include <iostream>
#include <vector>
#include <queue>
#include "Game.h"

using namespace ng;

// ---------------------------------------------------------------------------
//  Mini framework de test
// ---------------------------------------------------------------------------

namespace {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

int g_checksInTest = 0;
int g_failedInTest = 0;

struct TestFailure : std::exception {};

const char* resultName(Result r) {
    switch (r) {
        case Result::Ok:                 return "Ok";
        case Result::WrongPhase:         return "WrongPhase";
        case Result::OutOfBounds:        return "OutOfBounds";
        case Result::OccupiedCell:       return "OccupiedCell";
        case Result::NotConnected:       return "NotConnected";
        case Result::BaseOverlap:        return "BaseOverlap";
        case Result::BaseTooClose:       return "BaseTooClose";
        case Result::BaseNotAccessible:  return "BaseNotAccessible";
        case Result::NeedAtLeastOne:     return "NeedAtLeastOne";
        case Result::MustRelocateBase:   return "MustRelocateBase";
        case Result::GameOver:           return "GameOver";
        default:                         return "?";
    }
}

static void dumpBoard(const ng::Board& b) {
    for (int y = 0; y < b.height(); ++y) {
        for (int x = 0; x < b.width(); ++x) {
            ng::Cell c = b.get(x, y);
            char ch = '.';
            switch (c) {
                case ng::Cell::Empty:   ch = '.'; break;
                case ng::Cell::Square1: ch = 'o'; break;
                case ng::Cell::Square2: ch = 'x'; break;
                case ng::Cell::Base1:   ch = 'B'; break;
                case ng::Cell::Base2:   ch = 'b'; break;
                default:                ch = '?'; break;
            }
            std::cerr << ch;
        }
        std::cerr << '\n';
    }
    std::cerr << '\n';
}

void reportFailure(const char* file, int line, const std::string& msg) {
    ++g_failedInTest;
    std::printf("    [%s:%d] %s\n", file, line, msg.c_str());
}

}  // namespace

#define TEST(name)                                                    \
    static void name();                                               \
    static Registrar reg_##name(#name, name);                         \
    static void name()

#define CHECK(cond)                                                   \
    do {                                                              \
        ++g_checksInTest;                                             \
        if (!(cond)) reportFailure(__FILE__, __LINE__,                \
                                   "CHECK(" #cond ") a echoue");      \
    } while (0)

#define CHECK_EQ(actual, expected)                                    \
    do {                                                              \
        ++g_checksInTest;                                             \
        auto _a = (actual);                                           \
        auto _e = (expected);                                         \
        if (!(_a == _e)) reportFailure(__FILE__, __LINE__,            \
            std::string("attendu ") + #expected + " = " +             \
            std::to_string(static_cast<long long>(_e)) +              \
            ", obtenu " + #actual + " = " +                           \
            std::to_string(static_cast<long long>(_a)));              \
    } while (0)

#define CHECK_RESULT(actual, expected)                                \
    do {                                                              \
        ++g_checksInTest;                                             \
        Result _a = (actual);                                         \
        Result _e = (expected);                                       \
        if (_a != _e) reportFailure(__FILE__, __LINE__,               \
            std::string("attendu ") + resultName(_e) +                \
            ", obtenu " + resultName(_a) + " (" #actual ")");         \
    } while (0)

#define CHECK_THROWS(expr)                                            \
    do {                                                              \
        ++g_checksInTest;                                             \
        bool _threw = false;                                          \
        try { expr; } catch (...) { _threw = true; }                  \
        if (!_threw) reportFailure(__FILE__, __LINE__,                \
                                   "exception attendue : " #expr);    \
    } while (0)

#define CHECK_NOTHROW(expr)                                           \
    do {                                                              \
        ++g_checksInTest;                                             \
        try { expr; } catch (...) {                                   \
            reportFailure(__FILE__, __LINE__,                         \
                          "exception inattendue : " #expr);           \
        }                                                             \
    } while (0)

// ---------------------------------------------------------------------------
//  Fixtures
// ---------------------------------------------------------------------------

namespace {

Game makeFarGame() {
    Game g(20, 20);
    CHECK_RESULT(g.placeBase(Pos{2, 9}), Result::Ok);    // J1
    CHECK_RESULT(g.placeBase(Pos{17, 9}), Result::Ok);   // J2
    return g;
}

Game makeCloseGame() {
    Game g(20, 20);
    CHECK_RESULT(g.placeBase(Pos{2, 9}), Result::Ok);    // J1
    CHECK_RESULT(g.placeBase(Pos{9, 9}), Result::Ok);    // J2
    return g;
}

void putBases(Board& b, int j2x, int j2y, int j1x, int j1y) {
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
            b.set(j2x + dx, j2y + dy, baseOf(Player::Two));
            b.set(j1x + dx, j1y + dy, baseOf(Player::One));
        }
}

void j1PlaceAndPass(Game& g, int x, int y = 15) {
    CHECK_RESULT(g.placeSquare(Pos{x, y}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);
}

void j2PlaceAndPass(Game& g, int x) {
    CHECK_RESULT(g.placeSquare(Pos{x, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);
}

Game makeEnclosureReady() {
    Game g(20, 20);
    CHECK_RESULT(g.placeBase(Pos{15, 15}), Result::Ok);
    CHECK_RESULT(g.placeBase(Pos{0, 0}), Result::Ok);
    j1PlaceAndPass(g, 14);
    CHECK_RESULT(g.placeSquare(Pos{2, 0}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{2, 1}), Result::Ok);
    j1PlaceAndPass(g, 13);
    CHECK_RESULT(g.placeSquare(Pos{2, 2}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{1, 2}), Result::Ok);
    j1PlaceAndPass(g, 12);
    CHECK_RESULT(g.placeSquare(Pos{0, 2}), Result::Ok);
    return g;
}

}  // namespace

// ===========================================================================
//  Mise en place
// ===========================================================================

TEST(grille_taille_limites_invalides) {
    CHECK_THROWS(Game(14, 20));
    CHECK_THROWS(Game(20, 101));
}

TEST(grille_taille_limites_valides) {
    CHECK_NOTHROW(Game(15, 100));
}

TEST(base_bord_autorisee) {
    Game g(20, 20);
    CHECK_RESULT(g.placeBase(Pos{0, 0}), Result::Ok);
}

TEST(base_hors_grille_refusee) {
    Game g(20, 20);
    CHECK_RESULT(g.placeBase(Pos{19, 19}), Result::BaseOverlap);
}

TEST(base_distance_minimale_5) {
    Game g(30, 30);
    CHECK_RESULT(g.placeBase(Pos{5, 5}), Result::Ok);
    CHECK_RESULT(g.placeBase(Pos{11, 5}), Result::BaseTooClose);
    CHECK_RESULT(g.placeBase(Pos{12, 5}), Result::Ok);
}

TEST(base_distance_diagonale_ok) {
    Game g(30, 30);
    CHECK_RESULT(g.placeBase(Pos{5, 5}), Result::Ok);
    CHECK_RESULT(g.placeBase(Pos{12, 12}), Result::Ok);
}

TEST(base_chevauchement_refuse) {
    Game g(30, 30);
    CHECK_RESULT(g.placeBase(Pos{5, 5}), Result::Ok);
    CHECK_RESULT(g.placeBase(Pos{6, 6}), Result::BaseOverlap);
}

TEST(j1_joue_en_premier_avec_4_pv) {
    Game g = makeFarGame();
    CHECK(g.phase() == Phase::PlaySquares);
    CHECK(g.current() == Player::One);
    CHECK_EQ(g.hp(Player::One), 4);
    CHECK_EQ(g.hp(Player::Two), 4);
}

TEST(pose_refusee_pendant_mise_en_place) {
    Game g(20, 20);
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::WrongPhase);
    CHECK_RESULT(g.endTurn(), Result::WrongPhase);
}

TEST(placement_base_refuse_en_jeu) {
    Game g = makeFarGame();
    CHECK_RESULT(g.placeBase(Pos{8, 3}), Result::WrongPhase);
}

TEST(placement_initial_respecte_distance) {
    Game g(30, 30);
    CHECK_RESULT(g.placeBase(Pos{5, 5}), Result::Ok);
    auto legal = g.legalInitialBasePlacements();
    CHECK(!legal.empty());
    for (const Pos& p : legal) {
        bool tooClose = p.x >= 7 && p.x <= 11 && p.y >= 3 && p.y <= 8;
        CHECK(!tooClose);
    }
}

// ===========================================================================
//  Pose de carrés
// ===========================================================================

TEST(pose_adjacente_par_un_cote) {
    Game g = makeFarGame();
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{4, 10}), Result::Ok);
}

TEST(pose_en_coin_seul_refusee) {
    Game g = makeFarGame();
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{5, 10}), Result::NotConnected);
    CHECK_EQ(g.placedThisTurn(), 1);
}

TEST(pose_non_connectee_refusee) {
    Game g = makeFarGame();
    CHECK_RESULT(g.placeSquare(Pos{8, 9}), Result::NotConnected);
}

TEST(pose_case_occupee_refusee) {
    Game g = makeFarGame();
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::OccupiedCell);
    CHECK_RESULT(g.placeSquare(Pos{2, 9}), Result::OccupiedCell);
}

TEST(pose_hors_grille_refusee) {
    Game g = makeFarGame();
    CHECK_RESULT(g.placeSquare(Pos{-1, 0}), Result::OutOfBounds);
    CHECK_RESULT(g.placeSquare(Pos{20, 0}), Result::OutOfBounds);
}

TEST(deuxieme_carre_touche_le_premier) {
    Game g = makeFarGame();
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{5, 9}), Result::Ok);
}

TEST(deux_poses_puis_passage_automatique) {
    Game g = makeFarGame();
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{5, 9}), Result::Ok);
    CHECK(g.current() == Player::Two);
    CHECK_EQ(g.placedThisTurn(), 0);
}

TEST(compteur_poses_du_tour) {
    Game g = makeFarGame();
    CHECK_EQ(g.placedThisTurn(), 0);
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_EQ(g.placedThisTurn(), 1);
}

TEST(pose_refusee_ne_consomme_pas_le_tour) {
    Game g = makeFarGame();
    CHECK_RESULT(g.placeSquare(Pos{8, 9}), Result::NotConnected);
    CHECK_EQ(g.placedThisTurn(), 0);
    CHECK(g.current() == Player::One);
}

TEST(fin_de_tour_sans_pose_interdite) {
    Game g = makeFarGame();
    CHECK_RESULT(g.endTurn(), Result::NeedAtLeastOne);
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);
    CHECK(g.current() == Player::Two);
}

TEST(legal_placements_non_vide_au_depart) {
    Game g = makeFarGame();
    CHECK(!g.legalSquarePlacements().empty());
}

TEST(pas_de_gagnant_au_depart) {
    Game g = makeFarGame();
    CHECK(!g.isOver());
}

// ===========================================================================
//  Dégâts et victoire
// ===========================================================================

TEST(degats_contact_base_adverse) {
    Game g = makeCloseGame();

    CHECK_RESULT(g.placeSquare(Pos{2, 11}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{3, 11}), Result::Ok);
    CHECK(g.current() == Player::Two);

    const int xs[] = {8, 7, 6, 5, 4};
    int xj1 = 4;                        // prochaine colonne libre pour J1 sur y=11
    for (std::size_t i = 0; i < sizeof(xs) / sizeof(xs[0]); ++i) {
        Result r = g.placeSquare(Pos{xs[i], 9});
        CHECK_RESULT(r, Result::Ok);
        if (r != Result::Ok) return;
        if (g.current() == Player::One) {
            CHECK_RESULT(g.placeSquare(Pos{xj1, 11}), Result::Ok);
            CHECK_RESULT(g.placeSquare(Pos{xj1 + 1, 11}), Result::Ok);
            xj1 += 2;                   // avance de 2 colonnes après 2 poses
            CHECK(g.current() == Player::Two);
        }
    }

    CHECK_EQ(g.hp(Player::One), 3);
}

TEST(degats_n_atteignent_pas_l_attaquant) {
    Game g = makeCloseGame();
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{5, 9}), Result::Ok);
    CHECK_EQ(g.hp(Player::One), 4);
    CHECK_EQ(g.hp(Player::Two), 4);
}

TEST(victoire_a_zero_pv) {
    Game g = makeCloseGame();
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{5, 9}), Result::Ok);
    j2PlaceAndPass(g, 11);
    CHECK_RESULT(g.placeSquare(Pos{6, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{7, 9}), Result::Ok);
    j2PlaceAndPass(g, 12);
    CHECK_RESULT(g.placeSquare(Pos{8, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{8, 10}), Result::Ok);
    j2PlaceAndPass(g, 13);
    CHECK_RESULT(g.placeSquare(Pos{8, 11}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{8, 8}), Result::Ok);
    CHECK_EQ(g.hp(Player::Two), 0); 
    CHECK(g.isOver());
    CHECK(g.winner() == Player::One);
}

TEST(apres_victoire_plus_de_pose) {
    Game g = makeCloseGame();
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{5, 9}), Result::Ok);
    j2PlaceAndPass(g, 11);
    CHECK_RESULT(g.placeSquare(Pos{6, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{7, 9}), Result::Ok);
    j2PlaceAndPass(g, 12);
    CHECK_RESULT(g.placeSquare(Pos{8, 9}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{8, 10}), Result::Ok);
    j2PlaceAndPass(g, 13);
    CHECK_RESULT(g.placeSquare(Pos{8, 11}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{8, 8}), Result::Ok);
    CHECK(g.isOver());
    CHECK_RESULT(g.placeSquare(Pos{7, 8}), Result::GameOver);
    CHECK_RESULT(g.endTurn(), Result::GameOver);
}

// ===========================================================================
//  Traversée de mur (règles)
// ===========================================================================

TEST(traversee_mur_fin_autorisee) {
    Board b(20, 20);
    putBases(b, 12, 9, 2, 9);
    b.set(5, 9, squareOf(Player::One));
    b.set(6, 9, squareOf(Player::Two));
    b.set(6, 10, squareOf(Player::Two));
    CHECK(rules::canPlaceSquare(b, Player::One, Pos{7, 9}, nullptr));
}

TEST(traversee_mur_epais_bloquee) {
    Board b(20, 20);
    putBases(b, 12, 9, 2, 9);
    b.set(5, 9, squareOf(Player::One));
    b.set(6, 9, squareOf(Player::Two));
    b.set(7, 9, squareOf(Player::Two));
    CHECK(!rules::canPlaceSquare(b, Player::One, Pos{8, 9}, nullptr));
}

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

// ===========================================================================
//  Enfermement (règles)
// ===========================================================================

TEST(bases_libres_non_enfermees) {
    Board b(20, 20);
    putBases(b, 15, 9, 2, 9);
    CHECK(rules::isBaseProtected(b, Player::One));
    CHECK(rules::isBaseProtected(b, Player::Two));
    CHECK(!rules::isEnclosed(b, Player::One));
    CHECK(!rules::isEnclosed(b, Player::Two));
}

TEST(mur_epaisseur_1_base_accessible) {
    Board b(20, 20);
    putBases(b, 0, 0, 15, 15);
    for (int y = 0; y <= 3; ++y) b.set(3, y, squareOf(Player::Two));
    for (int x = 0; x <= 3; ++x) b.set(x, 3, squareOf(Player::Two));
    b.set(4, 0, squareOf(Player::One));
    b.set(4, 1, squareOf(Player::One));
    CHECK(rules::isBaseProtected(b, Player::Two));
    CHECK(!rules::isEnclosed(b, Player::Two));
}

TEST(mur_epaisseur_2_base_enfermee) {
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
    CHECK(!rules::isBaseProtected(b, Player::Two));
    CHECK(rules::isEnclosed(b, Player::Two));
}

TEST(contact_direct_base_accessible) {
    Board b(20, 20);
    putBases(b, 5, 5, 15, 15);
    b.set(7, 5, squareOf(Player::One));
    CHECK(rules::isBaseProtected(b, Player::Two));
}

TEST(contact_diagonal_base_accessible) {
    Board b(20, 20);
    putBases(b, 5, 5, 15, 15);
    b.set(7, 7, squareOf(Player::One));
    CHECK(rules::isBaseProtected(b, Player::Two));
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
    CHECK(!rules::isBaseProtected(b, Player::Two));
    CHECK(rules::isEnclosed(b, Player::Two));
}

TEST(poche_fine_accessible_par_traversee) {
    Board b(20, 20);
    putBases(b, 0, 0, 15, 15);
    for (int k = 0; k <= 3; ++k) {
        b.set(3, k, squareOf(Player::Two));
        b.set(k, 3, squareOf(Player::Two));
    }
    b.set(4, 1, squareOf(Player::One));
    CHECK(rules::isBaseProtected(b, Player::Two));
}

TEST(symetrie_j1_enferme) {
    Board b(20, 20);
    putBases(b, 15, 15, 0, 0);
    for (int y = 0; y <= 4; ++y) {
        b.set(3, y, squareOf(Player::One));
        b.set(4, y, squareOf(Player::One));
    }
    for (int x = 0; x <= 4; ++x) {
        b.set(x, 3, squareOf(Player::One));
        b.set(x, 4, squareOf(Player::One));
    }
    for (int k = 0; k <= 3; ++k) {
        b.set(5, k, squareOf(Player::Two));
        b.set(k, 5, squareOf(Player::Two));
    }
    CHECK(!rules::isBaseProtected(b, Player::One));
    CHECK(rules::isEnclosed(b, Player::One));
}

TEST(base_coin_grille_sans_debordement) {
    Board b(20, 20);
    putBases(b, 0, 0, 18, 18);
    CHECK(rules::isBaseProtected(b, Player::Two));
    CHECK(!rules::isEnclosed(b, Player::Two));
}

// ===========================================================================
//  Déplacement de base forcé
// ===========================================================================

TEST(enfermement_declenche_relocalisation) {
    Game g = makeEnclosureReady();
    CHECK(g.phase() == Phase::RelocateBase);
    CHECK(g.mustRelocatePlayer() == Player::Two);
    CHECK(!g.legalBaseRelocations().empty());
}

TEST(relocations_vides_hors_phase) {
    Game g = makeFarGame();
    CHECK(g.legalBaseRelocations().empty());
    CHECK_RESULT(g.relocateBase(Pos{5, 5}), Result::WrongPhase);
}

TEST(deplacement_valide_retour_en_jeu) {
    Game g = makeEnclosureReady();

    auto opts = g.legalBaseRelocations();
    CHECK(!opts.empty());          // REQUIRE au lieu de CHECK : stoppe le test
    CHECK(g.relocateBase(opts[0]) == Result::Ok);
}

TEST(toutes_relocations_annoncees_acceptees) {
    Game base = makeEnclosureReady();
    auto opts = base.legalBaseRelocations();
    CHECK(!opts.empty());
    for (Pos p : opts) {
        Game g = base;
        CHECK_RESULT(g.relocateBase(p), Result::Ok);
    }
}

TEST(deplacement_trop_proche_refuse) {
    Game g = makeEnclosureReady();
    CHECK_RESULT(g.relocateBase(Pos{12, 12}), Result::BaseTooClose);
    CHECK(g.phase() == Phase::RelocateBase);
}

TEST(deplacement_sur_case_occupee_refuse) {
    Game g = makeEnclosureReady();
    CHECK_RESULT(g.relocateBase(Pos{14, 14}), Result::BaseOverlap);
    CHECK(g.phase() == Phase::RelocateBase);
}

TEST(deplacement_hors_grille_refuse) {
    Game g = makeEnclosureReady();
    CHECK_RESULT(g.relocateBase(Pos{19, 19}), Result::BaseOverlap);
    CHECK(g.phase() == Phase::RelocateBase);
}

TEST(pose_interdite_pendant_relocalisation) {
    Game g = makeEnclosureReady();
    CHECK_RESULT(g.placeSquare(Pos{5, 5}), Result::MustRelocateBase);
    CHECK_RESULT(g.endTurn(), Result::MustRelocateBase);
}

TEST(ancienne_base_devient_carres) {
    Game g = makeEnclosureReady();
    auto opts = g.legalBaseRelocations();
    CHECK(!opts.empty());
    CHECK_RESULT(g.relocateBase(opts[0]), Result::Ok);
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx)
            CHECK(g.board().get(Pos{dx, dy}) == squareOf(Player::Two));
}

TEST(apres_relocation_plus_enfermee) {
    Game g = makeEnclosureReady();
    auto opts = g.legalBaseRelocations();
    CHECK_RESULT(g.relocateBase(opts[0]), Result::Ok);
    CHECK(!rules::isEnclosed(g.board(), Player::Two));
}

TEST(traversee_mur_fin_minimal) {
    Game g = makeCloseGame();          // J1 base x=2..3 ; J2 base x=9..10, y=9..10

    // Tour J1
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);

    // Tour J2 : pose connectée à SA base
    CHECK_RESULT(g.placeSquare(Pos{8, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);

    // Tour J1
    CHECK_RESULT(g.placeSquare(Pos{5, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);

    // Tour J2 : prolonge vers le mur
    CHECK_RESULT(g.placeSquare(Pos{7, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);

    // Tour J1 : traverse le mur
    CHECK_RESULT(g.placeSquare(Pos{6, 9}), Result::Ok);  // à vérifier selon les règles
    CHECK_RESULT(g.endTurn(), Result::Ok);
}

// ===========================================================================

int main() {
    int testsFailed = 0;
    int checksTotal = 0;
    int checksFailed = 0;

    for (const TestCase& t : registry()) {
        g_checksInTest = 0;
        g_failedInTest = 0;
        std::printf("- %s\n", t.name);
        try {
            t.body();
        } catch (const std::exception& e) {
            ++g_failedInTest;
            std::printf("    exception std : %s\n", e.what());
        } catch (...) {
            ++g_failedInTest;
            std::printf("    exception inconnue\n");
        }
        checksTotal += g_checksInTest;
        checksFailed += g_failedInTest;
        if (g_failedInTest > 0) ++testsFailed;
    }

    std::printf("\n%zu tests, %d en echec | %d verifications, %d echec(s)\n",
                registry().size(), testsFailed, checksTotal, checksFailed);
    return testsFailed == 0 ? 0 : 1;
}