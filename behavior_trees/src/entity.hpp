#pragma once
#include "common.hpp"

struct Entity final{
    static constexpr float min_speed = 24.0f;
    static constexpr float max_speed = 200.0f;
    static constexpr float hunger_per_second = 0.04f;
    static constexpr float drag = 0.01f;
    static constexpr float seek_weight = 1.0f;
    static constexpr float flee_weight = 1.2f;

    // patrol mission
    int waypoint_index = GetRandomValue(0, 3);    
    std::array<int, 8> bt_mem{};

    // hunger mission
    float hunger = random_range(0.0f, 1.0f);
    bool isHungry = false;

    std::string_view debug_state = "None";    
    Vector2 position = random_range(ZERO, STAGE_SIZE);
    Vector2 acceleration = ZERO;
    Vector2 velocity = vector_from_angle(random_range(0.0f, 2.0f * PI), min_speed);

    void update(float dt) noexcept{
        hunger = std::clamp(hunger + hunger_per_second * dt, 0.0f, 1.0f);
        velocity += acceleration * dt;
        velocity = Vector2ClampValue(velocity, min_speed, max_speed);
        position += velocity * dt;
        position = wrap(position);
        acceleration = ZERO;        
    }

    void render() const noexcept{
        Vector2 local_x = (Vector2Length(velocity) != 0) ? Vector2Normalize(velocity) : Vector2{1, 0};
        Vector2 local_y = {-local_x.y, local_x.x};
        float L = ENTITY_SIZE;
        float H = ENTITY_SIZE;
        Vector2 tip = position + (local_x * L * 1.4f);
        Vector2 left = position - (local_x * L) + (local_y * H);
        Vector2 right = position - (local_x * L) - (local_y * H);
        auto alpha = 1.0f - hunger * 0.7f;
        DrawTriangle(tip, right, left, Fade(GREEN, alpha));
    }
};