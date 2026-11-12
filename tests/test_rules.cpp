#include <algorithm>
#include <cstdio>
#include <limits>
#include <cstdlib>
#include <exception>
#include <functional>
#include <string>
#include <iostream>
#include <vector>
#include <queue>
#include <random>
#include "Game.h"

using namespace ng;

namespace {
struct TestCase {
    std::string name;
    std::function<void()> body;
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) {
        registry().push_back(TestCase{name, std::move(body)});
    }
};

int g_checksInTest = 0;
int g_failedInTest = 0;
bool g_suppressExpectedFailureReport = false;

struct TestFailure : std::exception {};

const char* resultName(Result result) {
    switch (result) {
    case Result::Ok: return "Ok";
    case Result::WrongPhase: return "WrongPhase";
    case Result::OutOfBounds: return "OutOfBounds";
    case Result::OccupiedCell: return "OccupiedCell";
    case Result::NotConnected: return "NotConnected";
    case Result::BaseOverlap: return "BaseOverlap";
    case Result::BaseTooClose: return "BaseTooClose";
    case Result::BaseNotAccessible: return "BaseNotAccessible";
    case Result::NeedAtLeastOne: return "NeedAtLeastOne";
    case Result::MustRelocateBase: return "MustRelocateBase";
    case Result::GameOver: return "GameOver";
    }
    return "UnknownResult";
}

void dumpBoard(const Board& board) {
    for (int y = 0; y < board.height(); ++y) {
        for (int x = 0; x < board.width(); ++x) {
            const Cell cell = board.get(x, y);
            char c = '.';
            if (cell == squareOf(Player::One)) c = 'o';
            else if (cell == squareOf(Player::Two)) c = 'x';
            else if (cell == baseOf(Player::One)) c = 'B';
            else if (cell == baseOf(Player::Two)) c = 'b';
            std::cerr << c;
        }
        std::cerr << '\n';
    }
}

void reportFailure(const char* file, int line, const std::string& msg) {
    if (!g_suppressExpectedFailureReport) {
        ++g_failedInTest;
        std::printf("    [%s:%d] %s\n", file, line, msg.c_str());
    }
}

#define TEST(name) \
    static void test_##name(); \
    static Registrar registrar_##name(#name, test_##name); \
    static void test_##name()
#define CHECK(expr) do { \
    ++g_checksInTest; \
    if (!(expr)) reportFailure(__FILE__, __LINE__, "CHECK(" #expr ")"); \
} while (false)
#define REQUIRE(expr) do { \
    ++g_checksInTest; \
    if (!(expr)) { reportFailure(__FILE__, __LINE__, "REQUIRE(" #expr ")"); throw TestFailure(); } \
} while (false)
#define CHECK_EQ(a, b) do { \
    ++g_checksInTest; \
    if (!((a) == (b))) reportFailure(__FILE__, __LINE__, "CHECK_EQ(" #a ", " #b ")"); \
} while (false)
#define CHECK_RESULT(actual, expected) do { \
    ++g_checksInTest; \
    const Result checkResultActual = (actual); \
    const Result checkResultExpected = (expected); \
    if (checkResultActual != checkResultExpected) reportFailure(__FILE__, __LINE__, std::string("CHECK_RESULT: attendu ") + resultName(checkResultExpected) + ", obtenu " + resultName(checkResultActual)); \
} while (false)
#define REQUIRE_RESULT(actual, expected)                              \
    do {                                                              \
        Result _a = (actual);                                         \
        Result _e = (expected);                                       \
        if (_a != _e) {                                               \
            reportFailure(__FILE__, __LINE__, "REQUIRE_RESULT échoué");\
            throw TestFailure{};                                      \
        }                                                             \
    } while (0)
#define CHECK_THROWS(expr) do { \
    ++g_checksInTest; bool checkThrew = false; \
    const int failuresBeforeExpectedThrow = g_failedInTest; \
    const bool previousSuppressExpectedFailureReport = g_suppressExpectedFailureReport; \
    g_suppressExpectedFailureReport = true; \
    try { (void)(expr); } catch (...) { checkThrew = true; } \
    g_suppressExpectedFailureReport = previousSuppressExpectedFailureReport; \
    if (checkThrew) g_failedInTest = failuresBeforeExpectedThrow; \
    else reportFailure(__FILE__, __LINE__, "CHECK_THROWS(" #expr ")"); \
} while (false)
#define CHECK_NOTHROW(expr) do { \
    ++g_checksInTest; bool checkThrew = false; \
    try { (void)(expr); } catch (...) { checkThrew = true; } \
    if (checkThrew) reportFailure(__FILE__, __LINE__, "CHECK_NOTHROW(" #expr ")"); \
} while (false)

Game makeFarGame() {
    Game g(20, 20);
    CHECK_RESULT(g.placeBase(Pos{2, 9}), Result::Ok);
    CHECK_RESULT(g.placeBase(Pos{17, 9}), Result::Ok);
    return g;
}

Game makeCloseGame() {
    Game g(20, 20);
    CHECK_RESULT(g.placeBase(Pos{2, 9}), Result::Ok);
    CHECK_RESULT(g.placeBase(Pos{9, 9}), Result::Ok);
    return g;
}

void putBases(Board& b, int j2x, int j2y, int j1x, int j1y) {
    REQUIRE(b.width() >= 2 && b.height() >= 2);
    REQUIRE(j2x >= 0 && j2y >= 0 && j1x >= 0 && j1y >= 0);
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
            REQUIRE(b.inBounds(j2x + dx, j2y + dy));
            REQUIRE(b.inBounds(j1x + dx, j1y + dy));
        }
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

struct AsciiRejected : std::exception {};   // à définir AVANT boardFromAscii
                                            // (supprimez la définition de la ligne 627)

Board boardFromAscii(const std::vector<std::string>& rows) {
    // Validation des dimensions
    if (rows.empty() || rows.front().empty()) throw AsciiRejected{};
    if (rows.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw AsciiRejected{};
    if (rows.front().size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw AsciiRejected{};

    const int height = static_cast<int>(rows.size());
    const int width  = static_cast<int>(rows.front().size());

    // Première passe : toutes les lignes ont la même largeur, glyphes valides,
    // comptage et position des bases.
    int nB = 0, nb = 0, bx = -1, by = -1, tx = -1, ty = -1;
    for (int y = 0; y < height; ++y) {
        const std::string& row = rows[static_cast<std::size_t>(y)];
        if (row.size() != static_cast<std::size_t>(width)) throw AsciiRejected{};
        for (int x = 0; x < width; ++x) {
            switch (row[static_cast<std::size_t>(x)]) {
            case '.': case 'o': case 'x': break;
            case 'B': if (nB++ == 0) { bx = x; by = y; } break;
            case 'b': if (nb++ == 0) { tx = x; ty = y; } break;
            default: throw AsciiRejected{};   // glyphe inconnu
            }
        }
    }
    if (!(nB == 4 && nb == 4)) throw AsciiRejected{};

    // Chaque base doit former un carré 2x2 plein.
    auto checkBase = [&](char c, int x0, int y0) {
        if (!(x0 >= 0 && y0 >= 0 && x0 + 1 < width && y0 + 1 < height)) throw AsciiRejected{};
        for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx)
                if (rows[static_cast<std::size_t>(y0 + dy)][static_cast<std::size_t>(x0 + dx)] != c)
                    throw AsciiRejected{};
    };
    checkBase('B', bx, by);
    checkBase('b', tx, ty);

    // Construction du plateau
    Board b(width, height);
    REQUIRE(b.width() == width && b.height() == height);   // invariant interne : REQUIRE OK
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Cell cell = Cell::Empty;
            switch (rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]) {
            case '.': break;
            case 'o': cell = squareOf(Player::One); break;
            case 'x': cell = squareOf(Player::Two); break;
            case 'B': cell = baseOf(Player::One);   break;
            case 'b': cell = baseOf(Player::Two);   break;
            default: throw AsciiRejected{};
            }
            REQUIRE(b.inBounds(x, y));                  // invariant interne
            b.set(x, y, cell);
        }
    }
    return b;
}

// Oracle BFS indépendant et volontairement simple : marche orthogonale sur
// vides/carrés de `from`, contact côtés/coins avec la base cible, et saut
// orthogonal d'un carré adverse si l'arrivée est passable ou dans cette base.
// C'est une hypothèse de test, non une spécification confirmée de la règle.
bool floodReachesNaive(const Board& b, Player from, Player target) {
    const int w=b.width(), h=b.height();
    REQUIRE(w>0 && h>0);
    const std::size_t sw=static_cast<std::size_t>(w), sh=static_cast<std::size_t>(h);
    REQUIRE(sh <= std::numeric_limits<std::size_t>::max()/sw);
    const std::size_t area=sw*sh;
    std::vector<unsigned char> seen(area,0);
    REQUIRE(seen.size()==area);
    auto inBounds=[&](int x,int y){ return x>=0 && y>=0 && x<w && y<h && b.inBounds(x,y); };
    auto indexOf=[&](int x,int y){
        REQUIRE(inBounds(x,y));
        const std::size_t idx=static_cast<std::size_t>(y)*sw+static_cast<std::size_t>(x);
        REQUIRE(idx<seen.size());
        return idx;
    };
    auto passable=[&](int x,int y){
        if (!inBounds(x,y)) return false;
        const Cell c=b.get(x,y);
        return c==Cell::Empty || c==squareOf(from);
    };
    auto isTargetBase=[&](int x,int y){ return inBounds(x,y) && b.get(x,y)==baseOf(target); };
    std::queue<Pos> q;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        REQUIRE(inBounds(x,y));
        if(b.get(x,y)==baseOf(from)) {
            const std::size_t idx=indexOf(x,y);
            if(!seen[idx]) { seen[idx]=1; q.push(Pos{x,y}); }
        }
    }
    const int dxs[4]={1,-1,0,0}, dys[4]={0,0,1,-1};
    while(!q.empty()) {
        const Pos p=q.front(); q.pop(); REQUIRE(inBounds(p.x,p.y));
        for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
            if((dx||dy) && isTargetBase(p.x+dx,p.y+dy)) return true;
        for(int i=0;i<4;++i) {
            const int nx=p.x+dxs[i], ny=p.y+dys[i];
            if(passable(nx,ny)) {
                const std::size_t idx=indexOf(nx,ny);
                if(!seen[idx]) { seen[idx]=1; q.push(Pos{nx,ny}); }
            }
            const int jx=nx, jy=ny, lx=p.x+2*dxs[i], ly=p.y+2*dys[i];
            if(inBounds(jx,jy) && b.get(jx,jy)==squareOf(opponent(from))) {
                if(isTargetBase(lx,ly)) return true;
                if(passable(lx,ly)) {
                    const std::size_t idx=indexOf(lx,ly);
                    if(!seen[idx]) { seen[idx]=1; q.push(Pos{lx,ly}); }
                }
            }
        }
    }
    return false;
}

Board randomBoard(std::mt19937& rng) {
    constexpr int width=20, height=20;
    Board b(width,height);
    REQUIRE(b.width()==width && b.height()==height);
    std::uniform_int_distribution<int> cornerCoord(0,4);
    std::uniform_int_distribution<int> farCornerCoord(15,18);
    const int j2x=cornerCoord(rng), j2y=cornerCoord(rng);
    // Pour une base 2x2, le coin supérieur gauche doit rester <= 18 sur 20.
    const int j1x=farCornerCoord(rng), j1y=farCornerCoord(rng);
    putBases(b,j2x,j2y,j1x,j1y);
    std::uniform_int_distribution<int> countDist(20,140), owner(0,1);
    const int count=countDist(rng);
    std::vector<Pos> available;
    available.reserve(static_cast<std::size_t>(width)*height-8);
    for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
        REQUIRE(b.inBounds(x,y));
        if(b.get(x,y)==Cell::Empty) available.push_back(Pos{x,y});
    }
    REQUIRE(available.size()>=static_cast<std::size_t>(count));
    for(int placed=0;placed<count;++placed) {
        std::uniform_int_distribution<std::size_t> choose(0,available.size()-1);
        const std::size_t index=choose(rng);
        REQUIRE(index<available.size());
        const Pos p=available[index];
        REQUIRE(b.inBounds(p.x,p.y) && b.get(p.x,p.y)==Cell::Empty);
        b.set(p.x,p.y,squareOf(owner(rng)==0 ? Player::One : Player::Two));
        available[index]=available.back();
        available.pop_back();
    }
    return b;
}

// Mise en place
TEST(grille_taille_limites_invalides) {
    CHECK_THROWS(Game(14, 20));
    CHECK_THROWS(Game(20, 101));
}
TEST(grille_taille_limites_valides) { CHECK_NOTHROW(Game(15, 100)); }
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
    CHECK_EQ(g.phase(), Phase::PlaySquares);
    CHECK_EQ(g.current(), Player::One);
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
    const auto opts = g.legalInitialBasePlacements();
    REQUIRE(!opts.empty());
    for (const Pos& p : opts)
        CHECK(!(p.x >= 7 && p.x <= 11 && p.y >= 3 && p.y <= 8));
}

// Pose de carrés (makeFarGame)
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
    CHECK_EQ(g.current(), Player::Two);
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
}
TEST(fin_de_tour_sans_pose_interdite) {
    Game g = makeFarGame();
    CHECK_RESULT(g.endTurn(), Result::NeedAtLeastOne);
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);
    CHECK_EQ(g.current(), Player::Two);
}
TEST(legal_placements_non_vide_au_depart) {
    Game g = makeFarGame();
    CHECK(!g.legalSquarePlacements().empty());
}
TEST(pas_de_gagnant_au_depart) {
    Game g = makeFarGame();
    CHECK(!g.winner().has_value());
}

// Dégâts et victoire (makeCloseGame)
TEST(degats_contact_base_adverse) {
    Game g = makeCloseGame();
    REQUIRE_RESULT(g.placeSquare(Pos{2, 11}), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{3, 11}), Result::Ok);
    int xj1 = 4;
    const int xs[] = {8, 7, 6, 5, 4};
    for (int x : xs) {
        REQUIRE_RESULT(g.placeSquare(Pos{x, 9}), Result::Ok);
        if (g.current() == Player::One) {
            CHECK_RESULT(g.placeSquare(Pos{xj1, 11}), Result::Ok);
            CHECK_RESULT(g.placeSquare(Pos{xj1 + 1, 11}), Result::Ok);
            xj1 += 2;
            CHECK_EQ(g.current(), Player::Two);
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
    CHECK_EQ(g.winner(), Player::One);
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
    REQUIRE(g.isOver());
    CHECK_RESULT(g.placeSquare(Pos{7, 8}), Result::GameOver);
    CHECK_RESULT(g.endTurn(), Result::GameOver);
}

// Traversée de mur
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

// Enfermement
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

TEST(board_ascii_validation_stricte) {
    auto rejected = [](std::vector<std::string> rows) {
        try { (void)boardFromAscii(rows); return false; }
        catch (const AsciiRejected&) { return true; }
    };
    CHECK(rejected({"bbx", "bb"}));                       // largeur non uniforme
    CHECK(rejected({"bb?", "bbx", "..BB", "..BB"}));      // glyphe inconnu
    CHECK(rejected({"b..", "...", "..B"}));               // bases incomplètes
    CHECK(rejected({"bb..", "bb..", "B...", "B..."}));    // 'B' en colonne 1 : 2 seulement, pas 4
    CHECK(!rejected({"bb..", "bb..", "..BB", "..BB"}));   // grille valide acceptée
    Board b = boardFromAscii({"bb..", "bb..", "..BB", "..BB"});
    CHECK(b.width() == 4 && b.height() == 4);
}

// Ajouter ici toute position fautive vue en jeu via dumpBoard.
TEST(regression_mur_fin_base_non_enfermee) {
    Board b(20, 20);
    putBases(b, 0, 0, 15, 15);
    for (int y = 0; y <= 3; ++y) b.set(3, y, squareOf(Player::Two));
    for (int x = 0; x <= 3; ++x) b.set(x, 3, squareOf(Player::Two));
    b.set(4, 0, squareOf(Player::One));
    b.set(4, 1, squareOf(Player::One));

    // La base de J1 (en (0,0)) est accessible depuis l'extérieur de la poche.
    CHECK(rules::isBaseProtected(b, Player::One));
    CHECK(!rules::isEnclosed(b, Player::One));
}

TEST(regression_mur_epais_base_enfermee) {
    Board b(20,20);
    putBases(b,0,0,15,15);
    for(int y=0;y<=3;++y) { b.set(2,y,squareOf(Player::Two)); b.set(3,y,squareOf(Player::Two)); }
    for(int x=0;x<=1;++x) { b.set(x,2,squareOf(Player::Two)); b.set(x,3,squareOf(Player::Two)); }
    for(int k=0;k<=3;++k) { b.set(4,k,squareOf(Player::One)); b.set(k,4,squareOf(Player::One)); }
    CHECK(!rules::isBaseProtected(b,Player::Two));
    CHECK(rules::isEnclosed(b,Player::Two));
}

// L'égalité avec rules::floodReaches reste désactivée : la sémantique métier
// des sauts/contact n'est pas suffisamment spécifiée dans ce fichier.
TEST(flood_oracle_naif_sans_comparaison_non_specifiee) {
    std::mt19937 rng(12345);
    for(int iteration=0;iteration<500;++iteration) {
        const Board b=randomBoard(rng);
        for(Player p : {Player::One,Player::Two}) {
            const bool first=floodReachesNaive(b,p,opponent(p));
            const bool second=floodReachesNaive(b,p,opponent(p));
            CHECK_EQ(first,second);
        }
    }
}

// Déplacement de base forcé
TEST(enfermement_declenche_relocalisation) {
    Game g = makeEnclosureReady();
    CHECK_EQ(g.phase(), Phase::RelocateBase);
    CHECK_EQ(g.mustRelocatePlayer(), Player::Two);
    CHECK(!g.legalBaseRelocations().empty());
}
TEST(relocations_vides_hors_phase) {
    Game g = makeFarGame();
    CHECK(g.legalBaseRelocations().empty());
    CHECK_RESULT(g.relocateBase(Pos{5, 5}), Result::WrongPhase);
}
TEST(deplacement_valide_retour_en_jeu) {
    Game g = makeEnclosureReady();
    const auto opts = g.legalBaseRelocations();
    REQUIRE(!opts.empty());
    CHECK_RESULT(g.relocateBase(opts[0]), Result::Ok);
    CHECK_EQ(g.phase(), Phase::PlaySquares);
}
TEST(toutes_relocations_annoncees_acceptees) {
    Game g = makeEnclosureReady();
    const auto opts = g.legalBaseRelocations();
    REQUIRE(!opts.empty());
    for (const Pos& p : opts) {
        Game copy = g;
        CHECK_RESULT(copy.relocateBase(p), Result::Ok);
    }
}
TEST(deplacement_trop_proche_refuse) {
    Game g = makeEnclosureReady();
    CHECK_RESULT(g.relocateBase(Pos{12, 12}), Result::BaseTooClose);
    CHECK_EQ(g.phase(), Phase::RelocateBase);
}
TEST(deplacement_sur_case_occupee_refuse) {
    Game g = makeEnclosureReady();
    CHECK_RESULT(g.relocateBase(Pos{14, 14}), Result::BaseOverlap);
}
TEST(deplacement_hors_grille_refuse) {
    Game g = makeEnclosureReady();
    CHECK_RESULT(g.relocateBase(Pos{19, 19}), Result::BaseOverlap);
}
TEST(pose_interdite_pendant_relocalisation) {
    Game g = makeEnclosureReady();
    CHECK_RESULT(g.placeSquare(Pos{5, 5}), Result::MustRelocateBase);
    CHECK_RESULT(g.endTurn(), Result::MustRelocateBase);
}
TEST(ancienne_base_devient_carres) {
    Game g = makeEnclosureReady();
    const auto opts = g.legalBaseRelocations();
    REQUIRE(!opts.empty());
    const Pos oldBase{0, 0};
    CHECK_RESULT(g.relocateBase(opts[0]), Result::Ok);
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx)
            CHECK_EQ(g.board().get(oldBase.x + dx, oldBase.y + dy), squareOf(Player::Two));
}
TEST(apres_relocation_plus_enfermee) {
    Game g = makeEnclosureReady();
    const auto opts = g.legalBaseRelocations();
    REQUIRE(!opts.empty());
    CHECK_RESULT(g.relocateBase(opts[0]), Result::Ok);
    CHECK(!rules::isEnclosed(g.board(), Player::Two));
}
TEST(traversee_mur_fin_minimal) {
    Game g = makeCloseGame();
    CHECK_RESULT(g.placeSquare(Pos{4, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{8, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{5, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{7, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);
    CHECK_RESULT(g.placeSquare(Pos{6, 9}), Result::Ok);
    CHECK_RESULT(g.endTurn(), Result::Ok);
}
// Invariants peu coûteux : vérifiés à chaque étape.
void checkInvariantsCheap(const Game& g, unsigned seed, int step) {
    auto verify = [&](bool condition, const char* label) {
        ++g_checksInTest;
        if (!condition)
            reportFailure(__FILE__, __LINE__, std::string("invariant ") + label +
                          " (seed=" + std::to_string(seed) + ", step=" + std::to_string(step) + ")");
    };
    int baseOneCount = 0, baseTwoCount = 0;
    for (int y = 0; y < g.board().height(); ++y)
        for (int x = 0; x < g.board().width(); ++x) {
            const Cell cell = g.board().get(x, y);
            if (cell == baseOf(Player::One)) ++baseOneCount;
            if (cell == baseOf(Player::Two)) ++baseTwoCount;
        }
    verify(baseOneCount == 4, "4 cases Base1");
    verify(baseTwoCount == 4, "4 cases Base2");
    verify(g.hp(Player::One) >= 0 && g.hp(Player::One) <= 4, "hp One dans [0,4]");
    verify(g.hp(Player::Two) >= 0 && g.hp(Player::Two) <= 4, "hp Two dans [0,4]");
}

// Invariants coûteux : seulement sur un petit échantillon de coups légaux.
void checkLegalMovesSampled(const Game& g, std::mt19937& rng, unsigned seed, int step) {
    constexpr std::size_t kSample = 3;
    if (g.phase() == Phase::PlaySquares) {
        const auto opts = g.legalSquarePlacements();
        for (std::size_t i = 0; i < std::min(kSample, opts.size()); ++i) {
            std::uniform_int_distribution<std::size_t> pick(0, opts.size() - 1);
            Game copy = g;
            const Result r = copy.placeSquare(opts[pick(rng)]);
            ++g_checksInTest;
            if (r != Result::Ok)
                reportFailure(__FILE__, __LINE__, "coup légal refusé (seed=" +
                              std::to_string(seed) + ", step=" + std::to_string(step) + ")");
        }
    } else if (g.phase() == Phase::RelocateBase) {
        const auto opts = g.legalBaseRelocations();
        for (std::size_t i = 0; i < std::min(kSample, opts.size()); ++i) {
            std::uniform_int_distribution<std::size_t> pick(0, opts.size() - 1);
            Game copy = g;
            const Result r = copy.relocateBase(opts[pick(rng)]);
            ++g_checksInTest;
            if (r != Result::Ok)
                reportFailure(__FILE__, __LINE__, "relocalisation légale refusée (seed=" +
                              std::to_string(seed) + ", step=" + std::to_string(step) + ")");
        }
    }
}

TEST(invariants_parties_aleatoires) {
    for (unsigned seed = 0; seed < 100; ++seed) {
        std::mt19937 rng(seed);
        const int size = 15 + static_cast<int>(seed % 10);
        Game g(size, size);
        auto initial = g.legalInitialBasePlacements();
        REQUIRE(!initial.empty());
        std::uniform_int_distribution<std::size_t> chooseInitial(0, initial.size() - 1);
        REQUIRE_RESULT(g.placeBase(initial[chooseInitial(rng)]), Result::Ok);
        initial = g.legalInitialBasePlacements();
        REQUIRE(!initial.empty());
        std::uniform_int_distribution<std::size_t> chooseSecond(0, initial.size() - 1);
        REQUIRE_RESULT(g.placeBase(initial[chooseSecond(rng)]), Result::Ok);

        for (int step = 0; step < 300 && !g.isOver(); ++step) {
            checkInvariantsCheap(g, seed, step);
            if (g.phase() == Phase::RelocateBase) {
                const auto options = g.legalBaseRelocations();
                if (options.empty()) break;
                std::uniform_int_distribution<std::size_t> choose(0, options.size() - 1);
                const Result r = g.relocateBase(options[choose(rng)]);
                if (r != Result::Ok) {
                    reportFailure(__FILE__, __LINE__, "relocate aleatoire refuse (seed=" +
                                  std::to_string(seed) + ", step=" + std::to_string(step) + ")");
                }
            } else if (g.phase() == Phase::PlaySquares) {
                const auto options = g.legalSquarePlacements();
                if (options.empty()) {
                    if (g.placedThisTurn() >= 1) {
                        const Result r = g.endTurn();
                        if (r != Result::Ok) {
                            reportFailure(__FILE__, __LINE__, "endTurn aleatoire refuse (seed=" +
                                          std::to_string(seed) + ", step=" + std::to_string(step) + ")");
                        }
                    } else break;
                } else {
                    std::uniform_int_distribution<std::size_t> choose(0, options.size() - 1);
                    const Result r = g.placeSquare(options[choose(rng)]);
                    if (r != Result::Ok) {
                        reportFailure(__FILE__, __LINE__, "placeSquare aleatoire refuse (seed=" +
                                      std::to_string(seed) + ", step=" + std::to_string(step) + ")");
                    }
                }
            } else break;
        }
    }
}

} // namespace

int main() {
    std::size_t checksTotal = 0;
    int checksFailed = 0;
    int testsFailed = 0;
    for (const TestCase& test : registry()) {
        g_checksInTest = 0;
        g_failedInTest = 0;
        std::printf("- %s\n", test.name.c_str());
        try {
            test.body();
        } catch (const TestFailure&) {
        } catch (const std::exception& e) {
            ++g_failedInTest;
            std::printf("    exception std: %s\n", e.what());
        } catch (...) {
            ++g_failedInTest;
            std::printf("    exception inconnue\n");
        }
        checksTotal += static_cast<std::size_t>(g_checksInTest);
        checksFailed += g_failedInTest;
        if (g_failedInTest != 0) ++testsFailed;
    }
    std::printf("\n%zu tests, %d en echec | %d verifications, %d echec(s)\n",
                registry().size(), testsFailed, static_cast<int>(checksTotal), checksFailed);
    return testsFailed == 0 ? 0 : 1;
}
