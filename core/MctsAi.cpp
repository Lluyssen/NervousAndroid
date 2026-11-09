#include "MctsAi.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace ng {

namespace {
constexpr int    kMaxCandidates = 24;    // coups explorés par nœud
constexpr int    kRolloutDepth  = 120;   // poses max par simulation
constexpr double kExplore       = 1.2;   // constante UCB1
constexpr double kGuidedProb    = 0.5;   // part de coups heuristiques en rollout
constexpr int    kRolloutSample = 6;     // taille de l'échantillon guidé
constexpr int    kScoreNoise    = 15;    // bruit ajouté au score de tri

int cheb(Pos a, Pos b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }
}  // namespace

MctsAi::MctsAi(unsigned seed) : rng_(seed) {}

// Joueur qui doit agir dans cet état.
Player MctsAi::actor(const Game& g) const {
    return g.phase() == Phase::RelocateBase ? g.mustRelocatePlayer() : g.current();
}

// Score heuristique d'une pose : attaque >> connexité >> approche >> défense.
int MctsAi::score(const Game& g, Pos c, Player who) const {
    const Board& b = g.board();
    const Player foe = opponent(who);
    int s = 0;

    // 1. Attaque : un carré qui touche la base adverse inflige des dégâts (R6).
    int dmg = rules::damageForPlacement(b, who, c);
    if (dmg > 0) {
        s += 100000;
        if (g.hp(foe) <= dmg) s += 1000000;   // coup gagnant immédiat
    }

    // 2. Connexité : nombre de voisins alliés (carrés ou base), 8 directions.
    int allies = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            if (!dx && !dy) continue;
            int nx = c.x + dx, ny = c.y + dy;
            if (b.inBounds(nx, ny) && b.isOwnedBy(nx, ny, who)) ++allies;
        }
    s += 30 * allies;

    // 3. Approche de la base adverse : pente faible, ne doit pas dominer.
    Pos foeBase;
    if (rules::findBase(b, foe, &foeBase)) s -= 3 * cheb(c, foeBase);

    // 4. Défense : si des carrés adverses sont proches de ma base,
    //    on privilégie les cases qui la protègent, borné.
    Pos myBase;
    if (rules::findBase(b, who, &myBase)) {
        int threat = 0;
        const int R = 3;
        for (int y = std::max(0, myBase.y - R); y <= std::min(b.height() - 1, myBase.y + R + 1); ++y)
            for (int x = std::max(0, myBase.x - R); x <= std::min(b.width() - 1, myBase.x + R + 1); ++x)
                if (b.isSquareOf(x, y, foe)) ++threat;
        if (threat > 0) {
            int near = cheb(c, myBase);
            s += std::min(threat, 10) * 5 - std::min(near, 10) * 3;
        }
    }
    return s;
}

// Évaluation d'une position non terminée, depuis le point de vue de 'root'.
// Renvoie une valeur dans [0, 1].
double MctsAi::evaluate(const Game& g, Player root) const {
    const Player foe = opponent(root);

    // 1. Écart de PV, saturé à ±5.
    int diff = g.hp(root) - g.hp(foe);
    double vHp = 0.5 + 0.5 * std::clamp(diff / 5.0, -1.0, 1.0);

    // 2. Mobilité : part des poses légales du joueur root.
    const int mRoot = static_cast<int>(g.legalSquarePlacementsFor(root).size());
    const int mFoe  = static_cast<int>(g.legalSquarePlacementsFor(foe).size());
    double vMob = 0.5;
    if (mRoot + mFoe > 0)
        vMob = static_cast<double>(mRoot) / static_cast<double>(mRoot + mFoe);

    // 3. Pression : bases proches = partie plus tendue.
    Pos rb, fb;
    double pressure = 0.0;
    if (rules::findBase(g.board(), root, &rb) && rules::findBase(g.board(), foe, &fb)) {
        int d = cheb(rb, fb);
        pressure = std::clamp(1.0 - d / 100.0, 0.0, 1.0);
    }
    double v = (1.0 - pressure) * (0.5 * vHp + 0.5 * vMob)
             + pressure         * (0.7 * vHp + 0.3 * vMob);
    return std::clamp(v, 0.0, 1.0);
}

// Liste de coups candidats (réduite) pour l'acteur courant.
std::vector<Move> MctsAi::candidateMoves(const Game& g) {
    std::vector<Move> out;

    if (g.phase() == Phase::PlaceBaseP1 || g.phase() == Phase::PlaceBaseP2) {
        for (const Pos& p : g.legalInitialBasePlacements())
            out.push_back({Move::Kind::Base, p});
        std::shuffle(out.begin(), out.end(), rng_);
        return out;
    }
    if (g.phase() == Phase::RelocateBase) {
        for (const Pos& p : g.legalBaseRelocations())
            out.push_back({Move::Kind::Relocate, p});
        std::shuffle(out.begin(), out.end(), rng_);
        return out;
    }
    if (g.phase() != Phase::PlaySquares) return out;

    Player who = g.current();
    std::vector<std::pair<int, Pos>> scored;
    std::uniform_int_distribution<int> noise(0, kScoreNoise);
    for (const Pos& p : g.legalSquarePlacements())
        scored.push_back({score(g, p, who) + noise(rng_), p});

    // Mélange avant le tri stable : les égalités sont départagées au hasard.
    std::shuffle(scored.begin(), scored.end(), rng_);
    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    if (static_cast<int>(scored.size()) > kMaxCandidates) scored.resize(kMaxCandidates);

    for (auto& sp : scored) out.push_back({Move::Kind::Square, sp.second});
    if (g.placedThisTurn() >= 1) out.push_back({Move::Kind::EndTurn, {0, 0}});
    return out;
}

bool MctsAi::apply(Game& g, const Move& m) {
    Result r = Result::WrongPhase;
    switch (m.kind) {
        case Move::Kind::Square:   r = g.placeSquare(m.pos);   break;
        case Move::Kind::EndTurn:  r = g.endTurn();            break;
        case Move::Kind::Base:     r = g.placeBase(m.pos);     break;
        case Move::Kind::Relocate: r = g.relocateBase(m.pos);  break;
    }
    return r == Result::Ok;
}

// Sélection UCB1 du meilleur enfant.
MctsAi::Node* MctsAi::select(Node* n) {
    Node* best = nullptr;
    double bestVal = -std::numeric_limits<double>::infinity();
    const double lnN = std::log(std::max(1, n->visits));
    for (auto& c : n->children) {
        double exploit = c->wins / std::max(1, c->visits);
        double explore = kExplore * std::sqrt(lnN / std::max(1, c->visits));
        double v = exploit + explore;
        if (v > bestVal) { bestVal = v; best = c.get(); }
    }
    return best;
}

// Ajoute un enfant pour un coup pas encore essayé, tiré au hasard.
MctsAi::Node* MctsAi::expand(Node* n) {
    while (!n->untried.empty()) {
        std::uniform_int_distribution<size_t> d(0, n->untried.size() - 1);
        const size_t i = d(rng_);
        Move m = n->untried[i];
        n->untried[i] = n->untried.back();
        n->untried.pop_back();

        auto child = std::make_unique<Node>(n->state);
        child->mover = actor(n->state);
        if (!apply(child->state, m)) continue;

        child->move = m;
        child->parent = n;
        child->untried = candidateMoves(child->state);
        Node* raw = child.get();
        n->children.push_back(std::move(child));
        return raw;
    }
    return nullptr;
}

// Coup de simulation : guidé (attaque/approche) ou aléatoire.
Move MctsAi::rolloutMove(const Game& g) {
    if (g.phase() == Phase::RelocateBase) {
        auto cells = g.legalBaseRelocations();
        if (cells.empty()) return {Move::Kind::EndTurn, {0, 0}};
        std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
        return {Move::Kind::Relocate, cells[d(rng_)]};
    }

    auto cells = g.legalSquarePlacements();
    if (cells.empty()) return {Move::Kind::EndTurn, {0, 0}};

    std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
    std::uniform_real_distribution<double> u(0.0, 1.0);

    if (u(rng_) < kGuidedProb) {
        const Player who = g.current();
        int bestS = std::numeric_limits<int>::min();
        Pos best = cells[d(rng_)];
        const int samples = std::min<int>(kRolloutSample, static_cast<int>(cells.size()));
        for (int i = 0; i < samples; ++i) {
            Pos c = cells[d(rng_)];
            int s = score(g, c, who);
            if (s > bestS) { bestS = s; best = c; }
        }
        return {Move::Kind::Square, best};
    }
    return {Move::Kind::Square, cells[d(rng_)]};
}

// Simule jusqu'à la fin ; renvoie 1 si 'root' gagne, 0 si perd,
// sinon l'évaluation heuristique de la position atteinte.
double MctsAi::rollout(Game g, Player root) {
    for (int i = 0; i < kRolloutDepth && !g.isOver(); ++i) {
        Move m = rolloutMove(g);
        if (!apply(g, m)) {
            if (g.phase() == Phase::PlaySquares && g.placedThisTurn() >= 1) g.endTurn();
            else break;
        }
    }
    if (g.isOver()) return g.winner() == root ? 1.0 : 0.0;
    return evaluate(g, root);
}

Move MctsAi::chooseMove(const Game& g, int timeBudgetMs) {
    using clock = std::chrono::steady_clock;
    const Player root = actor(g);

    auto rootNode = std::make_unique<Node>(g);
    rootNode->mover = opponent(root);
    rootNode->untried = candidateMoves(g);

    if (rootNode->untried.empty()) return {Move::Kind::EndTurn, {0, 0}};
    if (rootNode->untried.size() == 1) return rootNode->untried[0];

    const auto deadline = clock::now() + std::chrono::milliseconds(timeBudgetMs);
    lastIterations_ = 0;

    while (clock::now() < deadline) {
        Node* n = rootNode.get();

        // 1. Sélection
        while (n->untried.empty() && !n->children.empty() && !n->state.isOver())
            n = select(n);

        // 2. Expansion
        if (!n->untried.empty() && !n->state.isOver()) {
            Node* c = expand(n);
            if (c) n = c;
        }

        // 3. Simulation
        double res = rollout(n->state, root);

        // 4. Rétropropagation : le résultat est converti selon le joueur de chaque nœud.
        for (Node* p = n; p; p = p->parent) {
            p->visits++;
            p->wins += (p->mover == root) ? res : (1.0 - res);
        }
        ++lastIterations_;
    }

    // Meilleur coup = enfant le plus visité.
    Node* best = nullptr;
    for (auto& c : rootNode->children)
        if (!best || c->visits > best->visits) best = c.get();

    if (best) return best->move;
    return rootNode->untried.empty() ? Move{Move::Kind::EndTurn, {0, 0}}
                                     : rootNode->untried[0];
}

}  // namespace ng