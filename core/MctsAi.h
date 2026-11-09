#pragma once
#include <memory>
#include <random>
#include <vector>
#include "Game.h"

namespace ng {

// Un coup = poser un carrÃĐ, finir le tour, ou (re)placer une base.
struct Move {
    enum class Kind { Square, EndTurn, Base, Relocate };
    Kind kind = Kind::Square;
    Pos pos{0, 0};
};

class MctsAi {
public:
    explicit MctsAi(unsigned seed = std::random_device{}());

    // RÃĐflÃĐchit pendant timeBudgetMs et renvoie le meilleur coup pour g.current()
    // (ou mustRelocatePlayer() en phase RelocateBase).
    Move chooseMove(const Game& g, int timeBudgetMs);

    int lastIterations() const { return lastIterations_; }

private:
    struct Node {
        Game state;
        Move move;                          // coup menant Ã  ce nÅud
        Player mover;                       // joueur qui a jouÃĐ 'move'
        Node* parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
        std::vector<Move> untried;
        int visits = 0;
        double wins = 0.0;                  // du point de vue de 'mover'
        Node(const Game& s) : state(s), mover(Player::One) {}
    };

    Player actor(const Game& g) const;
    std::vector<Move> candidateMoves(const Game& g);
    static bool apply(Game& g, const Move& m);

    Node* select(Node* n);
    Node* expand(Node* n);
    double rollout(Game g, Player root);
    Move rolloutMove(const Game& g);

    int score(const Game& g, Pos c, Player who) const;
    double evaluate(const Game& g, Player root) const;

    std::mt19937 rng_;
    int lastIterations_ = 0;
};

}  // namespace ng