#pragma once
#include "common.hpp"

struct World final{
    Vector2 foodPos = {STAGE_WIDTH * 0.25f, STAGE_HEIGHT * 0.5f};
    Vector2 wolfPos = {STAGE_WIDTH * 0.75f, STAGE_HEIGHT * 0.5f};
    bool wolfActive = true;
    static constexpr float margin = ENTITY_SIZE * 10;
    std::array<Vector2, 4> waypoints{
        Vector2{margin, margin},
        Vector2{STAGE_WIDTH - margin, margin},
        Vector2{STAGE_WIDTH - margin, STAGE_HEIGHT - margin},
        Vector2{margin, STAGE_HEIGHT - margin}
    };

    void update(float dt) noexcept{
        static float t = 0.0f;
        t += dt;
        static constexpr Vector2 center{STAGE_WIDTH * 0.5f, STAGE_HEIGHT * 0.5f}; //origin of the motion        
        static constexpr Vector2 speed{0.7f, 1.1f};
        static constexpr Vector2 range{(STAGE_WIDTH * 0.28f), (STAGE_HEIGHT * 0.22f)}; //amplitude of the motion                        
        wolfPos.x = center.x + std::cos(t * speed.x) * range.x;
        wolfPos.y = center.y + std::sin(t * speed.y) * range.y;
    }

    void render() const noexcept{
        auto i = 0;
        for(auto node : waypoints){
            DrawCircleV(node, 6.0f, DARKGREEN);
            DrawText(TextFormat("%d", i++), node.x + 8.0f, node.y - 8.0f, FONT_SIZE, DARKGREEN);
        }
        DrawCircleV(foodPos, 10.0f, GOLD);
        if(wolfActive){ 
            DrawCircleV(wolfPos, 14.0f, RED); 
        }
        DrawText("F = toggle wolf", 10, 10, FONT_SIZE, DARKGRAY);
    }
};