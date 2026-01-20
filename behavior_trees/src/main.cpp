/**
 * Behavior Trees Demo
 * -------------
 * This code was written for educational purposes.
 * Original repository: https://github.com/ulfben/behavior_trees/
 * 
 * Copyright (c) 2026, Ulf Benjaminsson
 */
#include <codeanalysis\warnings.h>
#pragma warning(push)
#pragma warning(disable:ALL_CODE_ANALYSIS_WARNINGS)
#include "raylib.h"
#include "raymath.h"
#pragma warning(pop)
#include <concepts>
#include <cassert>
#include <string_view>
#include <span>
#include <vector>
#include <cmath>
#include <cstdlib>

constexpr int STAGE_WIDTH = 1280;
constexpr int STAGE_HEIGHT = 720;
constexpr int TARGET_FPS = 60;
constexpr int FONT_SIZE = 20;
constexpr Vector2 STAGE_SIZE = {static_cast<float>(STAGE_WIDTH), static_cast<float>(STAGE_HEIGHT)};
constexpr Vector2 ZERO = {0.0f, 0.0f};
constexpr auto CLEAR_COLOR = GRAY;
constexpr float TO_RAD = DEG2RAD;
constexpr float TO_DEG = RAD2DEG;

template <typename T, typename U>
constexpr T narrow_cast(U&& u) noexcept{
    return static_cast<T>(std::forward<U>(u));
}
constexpr static float to_float(int value) noexcept{
    return static_cast<float>(value);
}
inline float range01() noexcept{
    // We use 32767 (guaranteed minimum RAND_MAX per C++ standard) to ensure cross-platform consistency
    constexpr int resolution = 32767;
    const auto r = GetRandomValue(0, resolution); // inclusive
    return to_float(r) / (to_float(resolution) + 1.0f); // [0.0f,1.0f), exclusive
}

static float unit_range() noexcept{
    return (range01() * 2.0f) - 1.0f;
}

static float random_range(float min, float max) noexcept{
    assert(min < max);
    return min + ((max - min) * range01());
}

static Vector2 random_range(const Vector2& min, const Vector2& max) noexcept{
    return {
       random_range(min.x, max.x),
       random_range(min.y, max.y)
    };
}

static Vector2 vector_from_angle(float angle, float magnitude) noexcept{
    return {std::cos(angle) * magnitude, std::sin(angle) * magnitude};
}

struct Entity final{
    static constexpr float MIN_SPEED = 4.0f;
    static constexpr float MAX_SPEED = 40.0f;
    Vector2 position = random_range(ZERO, STAGE_SIZE);
    Vector2 velocity = vector_from_angle(random_range(0.0f, 360.0f) * TO_RAD, MIN_SPEED);
    
    void update(float deltaTime) noexcept{
        Vector2 acceleration = {0, 0};        
        velocity += acceleration * deltaTime;
        velocity = Vector2ClampValue(velocity, MIN_SPEED, MAX_SPEED);
        position += velocity * deltaTime;        
    }

    void render() const noexcept{
        Vector2 local_x = (Vector2Length(velocity) != 0) ? Vector2Normalize(velocity) : Vector2{1, 0};
        Vector2 local_y = {-local_x.y, local_x.x};
        float L = 10;
        float H = 10;
        Vector2 tip = position + (local_x * L * 1.4f);
        Vector2 left = position - (local_x * L) + (local_y * H);
        Vector2 right = position - (local_x * L) - (local_y * H);
        DrawTriangle(tip, right, left, GREEN);
    }   
};

struct Window final{
    Window(int width, int height, std::string_view title, int fps = TARGET_FPS){
        InitWindow(width, height, title.data());
        SetTargetFPS(fps);
    }
    ~Window() noexcept{
        CloseWindow();
    }

    void render(std::span<const Entity> entities) const noexcept{
        BeginDrawing();
        ClearBackground(CLEAR_COLOR);       
        for(const auto& e : entities){
            e.render();           
        }     
        DrawText("Press SPACE to pause/unpause", 10, STAGE_HEIGHT - FONT_SIZE, FONT_SIZE, DARKGRAY);
        DrawFPS(10, STAGE_HEIGHT - FONT_SIZE * 2);        
        EndDrawing();
    }

    bool should_close() const noexcept{
        return WindowShouldClose() || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q);
    }
};

int main(){
    auto window = Window(STAGE_WIDTH, STAGE_HEIGHT, "Behavior Tree Demo");    
    bool isPaused = false;
    std::vector<Entity> entities;
    while(!window.should_close()){
        float deltaTime = GetFrameTime();
        if(IsKeyPressed(KEY_SPACE)) isPaused = !isPaused;             

        for(auto& e : entities){           
            if(isPaused) continue;
            e.update(deltaTime);
        }

        window.render(entities);
    }
    return 0;
}