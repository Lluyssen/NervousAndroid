#include <jni.h>
#include <string>
#include "Game.h"

extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_nervousandroid_NativeBridge_selfTest(JNIEnv* env, jobject /*this*/) {
    ng::Game g(20, 20);
    auto r1 = g.placeBase(ng::Pos{2, 9});
    auto r2 = g.placeBase(ng::Pos{17, 9});
    auto r3 = g.placeSquare(ng::Pos{4, 9});
    std::string s = "Moteur C++ OK. Bases: " + ng::describe(r1) + " / " + ng::describe(r2) +
                    " | Pose: " + ng::describe(r3) +
                    " | PV J1=" + std::to_string(g.hp(ng::Player::One));
    return env->NewStringUTF(s.c_str());
}