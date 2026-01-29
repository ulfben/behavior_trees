#pragma once
#include "common.hpp"

struct World final{
    static constexpr float margin = ENTITY_SIZE * 10;
    static constexpr float waypoint_radius = 18.0f;
    static constexpr float food_radius = 16.0f;

    Vector2 food_pos = {STAGE_WIDTH * 0.25f, STAGE_HEIGHT * 0.5f};
    Vector2 wolf_pos = {STAGE_WIDTH * 0.75f, STAGE_HEIGHT * 0.5f};
    bool wolf_active = true;
    
    std::array<Vector2, 4> waypoints{
        Vector2{margin, margin},
        Vector2{STAGE_WIDTH - margin, margin},
        Vector2{STAGE_WIDTH - margin, STAGE_HEIGHT - margin},
        Vector2{margin, STAGE_HEIGHT - margin}
    };

    void respawn_food() noexcept{
        food_pos = random_range(ZERO, STAGE_SIZE);
    }        

    void update(float dt) noexcept{
        if(!wolf_active){ return; }
        static float t = 0.0f;
        t += dt;
        static constexpr Vector2 center{STAGE_WIDTH * 0.5f, STAGE_HEIGHT * 0.5f}; //origin of the motion        
        static constexpr Vector2 speed{0.7f, 1.1f};
        static constexpr Vector2 range{(STAGE_WIDTH * 0.28f), (STAGE_HEIGHT * 0.22f)}; //amplitude of the motion                        
        wolf_pos.x = center.x + std::cos(t * speed.x) * range.x;
        wolf_pos.y = center.y + std::sin(t * speed.y) * range.y;
    }

    void render() const noexcept{
        auto i = 0;
        for(auto node : waypoints){
            DrawCircleV(node, 6.0f, DARKGREEN);
            DrawText(TextFormat("%d", i++), node.x + 8.0f, node.y - 8.0f, FONT_SIZE, DARKGREEN);
        }
        DrawCircleV(food_pos, food_radius, GOLD);
        if(wolf_active){ 
            DrawCircleV(wolf_pos, 14.0f, RED); 
        }
        DrawText("F = toggle wolf", 10, 10, FONT_SIZE, DARKGRAY);
    }
};