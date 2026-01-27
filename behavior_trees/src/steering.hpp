#pragma once
#include "common.hpp"
#include "entity.hpp"

static Vector2 steer_seek(const Entity& e, Vector2 targetPos, float max_speed) noexcept{
    auto toward = Vector2Normalize(targetPos - e.position);
    auto desired_velocity = toward * max_speed;
    return (desired_velocity - e.velocity) * Entity::seek_weight;
}

static Vector2 steer_flee(const Entity& e, Vector2 threatPos, float max_speed) noexcept{
    Vector2 d = e.position - threatPos;
    if(Vector2LengthSqr(d) < 0.0001f) d = Vector2{1, 0};
    auto away = Vector2Normalize(d);
    auto desired_velocity = away * max_speed;
    return (desired_velocity - e.velocity) * Entity::flee_weight;
}

static Vector2 steer_drag(const Entity& e) noexcept{
    return e.velocity * -Entity::drag;
}