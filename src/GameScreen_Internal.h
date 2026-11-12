#pragma once

#include "GameScreen.h"
#include "Rules.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace ng {
namespace {
struct Rgba { Uint8 r, g, b, a; };

// Palette
inline constexpr Rgba kBg       {16, 18, 26, 255};
inline constexpr Rgba kPanel    {26, 29, 42, 255};
inline constexpr Rgba kGridCell {34, 38, 54, 255};
inline constexpr Rgba kText     {236, 239, 248, 255};
inline constexpr Rgba kTextDim  {140, 148, 172, 255};
inline constexpr Rgba kP1       {64, 140, 255, 255};
inline constexpr Rgba kP1Dark   {30, 80, 180, 255};
inline constexpr Rgba kP2       {255, 92, 108, 255};
inline constexpr Rgba kP2Dark   {180, 44, 64, 255};
inline constexpr Rgba kHint     {255, 236, 130, 95};
inline constexpr Rgba kGood     {80, 220, 140, 255};
inline constexpr Rgba kCard     {28, 32, 48, 255};
inline constexpr Rgba kIllegal  {255, 80, 80, 140};

// Mise en page HUD et temporisations IA
inline constexpr float kHudHeight = 90.f;
inline constexpr float kCardY = 8.f;
inline constexpr float kCardH = 66.f;
inline constexpr float kMargin = 12.f;
inline constexpr float kCardGap = 12.f;
inline constexpr float kStatusY = 78.f;
inline constexpr float kStatusH = 28.f;
inline constexpr int kAiThinkMs = 700;
inline constexpr Uint64 kEasyDelayMs = 350;

inline void setColor(SDL_Renderer* r, Rgba c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}
inline Rgba withAlpha(Rgba c, Uint8 a) { return Rgba{c.r, c.g, c.b, a}; }
inline Rgba playerColor(Player p) { return p == Player::One ? kP1 : kP2; }
}  // namespace
}  // namespace ng
