#pragma once

#include <codeanalysis\warnings.h>
#pragma warning(push)
#pragma warning(disable:ALL_CODE_ANALYSIS_WARNINGS)
#include "raylib.h"
#include "raymath.h"
#pragma warning(pop)

#include <cassert>
#include <string_view>
#include <span>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <tuple>

// --- Constants ---
constexpr int STAGE_WIDTH = 1280;
constexpr int STAGE_HEIGHT = 720;
constexpr int TARGET_FPS = 60;
constexpr int FONT_SIZE = 20;
constexpr Vector2 STAGE_SIZE = {static_cast<float>(STAGE_WIDTH), static_cast<float>(STAGE_HEIGHT)};
constexpr Vector2 ZERO = {0.0f, 0.0f};
constexpr auto CLEAR_COLOR = GRAY;
constexpr float ENTITY_SIZE = 10.0f;

// --- Math Helpers ---
static constexpr float to_float(int value) noexcept{
    return static_cast<float>(value);
}

static constexpr int to_int(float value) noexcept{
    return static_cast<int>(value);
}

inline float range01() noexcept{
    constexpr int resolution = 32767;
    const auto r = GetRandomValue(0, resolution);
    return to_float(r) / (to_float(resolution) + 1.0f);
}

static float random_range(float min, float max) noexcept{
    assert(min < max);
    return min + ((max - min) * range01());
}

static Vector2 random_range(const Vector2& min, const Vector2& max) noexcept{
    return {random_range(min.x, max.x), random_range(min.y, max.y)};
}

static Vector2 vector_from_angle(float angle, float magnitude) noexcept{
    return {std::cos(angle) * magnitude, std::sin(angle) * magnitude};
}

static void DrawText(const char* s, float x, float y, int size, Color color) noexcept{
    DrawText(s, to_int(x), to_int(y), size, color);
}


static constexpr Vector2 wrap(Vector2 p) noexcept{
    if(p.x < ENTITY_SIZE) p.x += STAGE_WIDTH;
    if(p.y < ENTITY_SIZE) p.y += STAGE_HEIGHT;
    if(p.x >= STAGE_WIDTH) p.x -= STAGE_WIDTH + ENTITY_SIZE;
    if(p.y >= STAGE_HEIGHT) p.y -= STAGE_HEIGHT + ENTITY_SIZE;
    return p;
}