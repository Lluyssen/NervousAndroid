#include <jni.h>
#include <memory>
#include <mutex>
#include <vector>
#include "Game.h"

using namespace ng;

namespace {
    std::unique_ptr<Game> g_game;
    std::mutex g_mutex;

    constexpr int kHeader = 9;

    jintArray toJInts(JNIEnv* env, const std::vector<jint>& v) {
        jintArray arr = env->NewIntArray(static_cast<jsize>(v.size()));
        if (arr && !v.empty())
            env->SetIntArrayRegion(arr, 0, static_cast<jsize>(v.size()), v.data());
        return arr;
    }

    std::vector<Pos> legalFor(const Game& g, int kind) {
        switch (kind) {
            case 0: return g.legalInitialBasePlacements();
            case 1: return g.legalSquarePlacements();
            case 3: return g.legalBaseRelocations();
            default: return {};
        }
    }
}

extern "C" {

// Crée une partie. Renvoie 0 si OK, 1 si dimensions invalides.
JNIEXPORT jint JNICALL
Java_com_nervous_game_NativeBridge_nativeNewGame(JNIEnv*, jobject, jint w, jint h) {
    std::lock_guard<std::mutex> lock(g_mutex);
    try {
        g_game = std::make_unique<Game>(w, h);
        return 0;
    } catch (const std::exception&) {
        g_game.reset();
        return 1;
    }
}

// type : 0 base, 1 carré, 2 fin de tour, 3 déplacement de base.
// Renvoie l'entier correspondant à ng::Result, ou -1 si aucune partie.
JNIEXPORT jint JNICALL
Java_com_nervous_game_NativeBridge_nativeAction(JNIEnv*, jobject, jint type, jint x, jint y) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_game) return -1;
    Result r = Result::WrongPhase;
    switch (type) {
        case 0: r = g_game->placeBase(Pos{x, y}); break;
        case 1: r = g_game->placeSquare(Pos{x, y}); break;
        case 2: r = g_game->endTurn(); break;
        case 3: r = g_game->relocateBase(Pos{x, y}); break;
        default: break;
    }
    return static_cast<jint>(r);
}

JNIEXPORT jintArray JNICALL
Java_com_nervous_game_NativeBridge_nativeGetState(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_game) return env->NewIntArray(0);

    const Board& b = g_game->board();
    std::vector<jint> out;
    out.reserve(kHeader + b.width() * b.height());

    out.push_back(b.width());
    out.push_back(b.height());
    out.push_back(static_cast<jint>(g_game->phase()));
    out.push_back(static_cast<jint>(g_game->current()));
    out.push_back(g_game->hp(Player::One));
    out.push_back(g_game->hp(Player::Two));
    out.push_back(g_game->placedThisTurn());
    out.push_back(static_cast<jint>(g_game->winner()));
    out.push_back(static_cast<jint>(g_game->mustRelocatePlayer()));

    for (int y = 0; y < b.height(); ++y)
        for (int x = 0; x < b.width(); ++x)
            out.push_back(static_cast<jint>(b.get(x, y)));

    return toJInts(env, out);
}

// kind : 0 bases initiales, 1 carrés, 3 déplacements. Résultat : x0,y0,x1,y1,...
JNIEXPORT jintArray JNICALL
Java_com_nervous_game_NativeBridge_nativeGetLegal(JNIEnv* env, jobject, jint kind) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_game) return env->NewIntArray(0);

    std::vector<Pos> pos = legalFor(*g_game, kind);
    std::vector<jint> out;
    out.reserve(pos.size() * 2);
    for (const Pos& p : pos) {
        out.push_back(p.x);
        out.push_back(p.y);
    }
    return toJInts(env, out);
}

JNIEXPORT jstring JNICALL
Java_com_nervous_game_NativeBridge_nativeDescribe(JNIEnv* env, jobject, jint code) {
    return env->NewStringUTF(describe(static_cast<Result>(code)).c_str());
}

}  // extern "C"