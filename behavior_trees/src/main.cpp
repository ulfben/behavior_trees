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
#include <stdexcept>
#include <algorithm>

constexpr int STAGE_WIDTH = 1280;
constexpr int STAGE_HEIGHT = 720;
constexpr int TARGET_FPS = 60;
constexpr int FONT_SIZE = 20;
constexpr Vector2 STAGE_SIZE = {static_cast<float>(STAGE_WIDTH), static_cast<float>(STAGE_HEIGHT)};
constexpr Vector2 ZERO = {0.0f, 0.0f};
constexpr auto CLEAR_COLOR = GRAY;
constexpr float TO_RAD = DEG2RAD;
constexpr float TO_DEG = RAD2DEG;
constexpr float ENTITY_SIZE = 10.0f;

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
    static constexpr float MIN_SPEED = 24.0f;
    static constexpr float MAX_SPEED = 200.0f;
    std::string_view debug_state = "None";
    static constexpr float wander_radius = 60.0f;
    static constexpr float wander_distance = 80.0f;
    static constexpr float wander_jitter = 4.0f;   // radians/sec-ish when multiplied by dt
    float wander_angle = random_range(0.0f, 2.0f * PI);

    Vector2 position = random_range(ZERO, STAGE_SIZE);
    Vector2 velocity = vector_from_angle(wander_angle, MIN_SPEED);

    void render() const noexcept{
        Vector2 local_x = (Vector2Length(velocity) != 0) ? Vector2Normalize(velocity) : Vector2{1, 0};
        Vector2 local_y = {-local_x.y, local_x.x};
        float L = ENTITY_SIZE;
        float H = ENTITY_SIZE;
        Vector2 tip = position + (local_x * L * 1.4f);
        Vector2 left = position - (local_x * L) + (local_y * H);
        Vector2 right = position - (local_x * L) - (local_y * H);
        DrawTriangle(tip, right, left, GREEN);
           // Debug: velocity vector (scaled for visibility)
        DrawLineV(position, position + velocity * 2.0f, BLUE);
    }   
};

enum class Status{ Success, Failure, Running };

struct Context;

// Base node interface
struct Node{
    virtual ~Node() = default;
    virtual Status tick(Context& ctx, float dt) const noexcept = 0;
};

// Composite: Runs children left-to-right.
// IF a child fails, the sequence Fails immediately.
// IF a child runs, the sequence returns Running (and restarts from 0 next frame).
struct Sequence final : Node{
    std::vector<Node*> children;

    explicit Sequence(std::initializer_list<Node*> xs) : children(xs){}

    Status tick(Context& ctx, float dt) const noexcept override{
        for(const auto* child : children){
            const Status s = child->tick(ctx, dt);

            // If child is running, we return Running. 
            // Crucially, we do NOT save the index. Next frame starts at 0 again.
            if(s == Status::Running) return Status::Running;

            if(s == Status::Failure) return Status::Failure;
        }
        return Status::Success;
    }
};

// Composite: Runs children left-to-right.
// IF a child succeeds, the selector Succeeds immediately.
// IF a child runs, the selector returns Running.
struct Selector final : Node{
    std::vector<Node*> children;

    explicit Selector(std::initializer_list<Node*> xs) : children(xs){}

    Status tick(Context& ctx, float dt) const noexcept override{
        for(const auto* child : children){
            const Status s = child->tick(ctx, dt);

            if(s == Status::Running) return Status::Running;
            if(s == Status::Success) return Status::Success;
        }
        return Status::Failure;
    }
};

// Leaf node: either a condition or an action, supplied as a function pointer.
// We opt for plain function pointer to enforce that leaf nodes are stateless; all behavior state lives in Context
using LeafFn = Status(*)(Context&, float) noexcept;

struct Leaf final : Node{
    LeafFn fn{};
    explicit Leaf(LeafFn f) : fn(f){}
    Status tick(Context& ctx, float dt) const noexcept override{ return fn(ctx, dt); }
};

static Vector2 wrap(Vector2 p) noexcept{
    if(p.x < ENTITY_SIZE) p.x += STAGE_WIDTH;
    if(p.y < ENTITY_SIZE) p.y += STAGE_HEIGHT;
    if(p.x >= STAGE_WIDTH) p.x -= STAGE_WIDTH+ENTITY_SIZE;
    if(p.y >= STAGE_HEIGHT) p.y -= STAGE_HEIGHT+ENTITY_SIZE;
    return p;
}

static Vector2 constrain(Vector2 p) noexcept{
    p.x = std::clamp(p.x, 0.0f, to_float(STAGE_WIDTH));
    p.y = std::clamp(p.y, 0.0f, to_float(STAGE_HEIGHT));    
    return p;
}

static Vector2 seek(Vector2 from, Vector2 to, float speed) noexcept{
    Vector2 d = to - from;
    if(Vector2LengthSqr(d) < 0.0001f) return ZERO;
    return Vector2Normalize(d) * speed;
}

static Vector2 flee(Vector2 from, Vector2 threat, float speed) noexcept{
    Vector2 d = from - threat;
    if(Vector2LengthSqr(d) < 0.0001f) return Vector2Normalize(Vector2{1, 0}) * speed;
    return Vector2Normalize(d) * speed;
}

struct World final{
    Vector2 foodPos = {STAGE_WIDTH * 0.25f, STAGE_HEIGHT * 0.5f};
    Vector2 wolfPos = {STAGE_WIDTH * 0.75f, STAGE_HEIGHT * 0.5f};
    bool wolfActive = true;

    void update(float dt) noexcept{
        // Let the "wolf" orbit to make the threat more dynamic
        static float t = 0.0f;
        t += dt;
        wolfPos.x = (STAGE_WIDTH * 0.5f) + std::cos(t * 0.7f) * (STAGE_WIDTH * 0.28f);
        wolfPos.y = (STAGE_HEIGHT * 0.5f) + std::sin(t * 1.1f) * (STAGE_HEIGHT * 0.22f);
    }

    void render() const noexcept{
        DrawCircleV(foodPos, 10.0f, GOLD);
        if(wolfActive) DrawCircleV(wolfPos, 14.0f, RED);
        DrawText("F = toggle wolf", 10, 10, FONT_SIZE, DARKGRAY);
    }
};

struct EntityBrain final{
    Node* root = nullptr;
    Status tick(Context& ctx, float dt) noexcept{
        assert(root);
        return root->tick(ctx, dt);
    }
};

struct Context final{
    Entity& self;
    World& world;
};

// ===========================
// Demo leaves (conditions/actions)
// ===========================
static Status ThreatNearby(Context& ctx, float) noexcept{
    if(!ctx.world.wolfActive) return Status::Failure;
    const float dist = Vector2Distance(ctx.self.position, ctx.world.wolfPos);
    return (dist < 180.0f) ? Status::Success : Status::Failure;
}

static Status Hungry(Context& ctx, float) noexcept{    
    const float x = ctx.self.position.x / STAGE_WIDTH;
    return (x < 0.35f) ? Status::Success : Status::Failure;
}

static Status DoFlee(Context& ctx, float) noexcept{
    ctx.self.debug_state = "FLEE";
    ctx.self.velocity = flee(ctx.self.position, ctx.world.wolfPos, Entity::MAX_SPEED);    
    return Status::Running;
}

static Status DoSeekFood(Context& ctx, float) noexcept{
    ctx.self.debug_state = "SEEK FOOD";
    ctx.self.velocity = seek(ctx.self.position, ctx.world.foodPos, Entity::MAX_SPEED * 0.7f);
    // Consider "success" once we reach food
    const float dist = Vector2Distance(ctx.self.position, ctx.world.foodPos);
    return (dist < 16.0f) ? Status::Success : Status::Running;
}

static Status DoWander(Context& ctx, float dt) noexcept{    
    ctx.self.debug_state = "WANDER";
    auto& entity = ctx.self;
    // Project a circle in front of the agent (based on current heading)
    Vector2 heading = Vector2Normalize(entity.velocity);
    Vector2 circle_center = heading * Entity::wander_distance;

    // Jitter the angle a little bit each tick (scale by dt for frame-rate independence)
    entity.wander_angle += unit_range() * Entity::wander_jitter * dt;

    // Displacement on the circle
    Vector2 displacement = {
        std::cos(entity.wander_angle) * Entity::wander_radius,
        std::sin(entity.wander_angle) * Entity::wander_radius
    };
    // The wander target in world space
    Vector2 target = entity.position + circle_center + displacement;
    auto toward = Vector2Normalize(target - entity.position);
    auto desired_velocity = toward * Entity::MIN_SPEED;
    ctx.self.velocity = Vector2Lerp(ctx.self.velocity, desired_velocity, 0.02f);
    return Status::Running;
}

struct DemoTree final{
    // Nodes live here so pointers stay valid (no heap, no ownership headaches).
    Leaf threat{ThreatNearby};
    Leaf hungry{Hungry};
    Leaf flee{DoFlee};
    Leaf seekFood{DoSeekFood};
    Leaf wander{DoWander};

    Sequence fleeSeq{&threat, &flee};
    Sequence foodSeq{&hungry, &seekFood};
    Selector root{&fleeSeq, &foodSeq, &wander};

    EntityBrain brain{&root};
};

//free function update, to avoid circular dependency problems 
static void update_entity(Entity& e, DemoTree& tree, World& world, float dt) noexcept{
    Context ctx{e, world};
    std::ignore = tree.brain.tick(ctx, dt);
    e.position += e.velocity * dt;
    e.position = constrain(e.position);  
}

struct Window final{
    Window(int width, int height, std::string_view title, int fps = 0){
        InitWindow(width, height, title.data());
		if(!IsWindowReady()){
			throw std::runtime_error("Unable to create Raylib window. Check settings?");
		}
		if(fps < 1){
			int hz = GetMonitorRefreshRate(GetCurrentMonitor());
			SetTargetFPS(hz > 0 ? hz : 60); // default to 60 FPS if monitor refresh rate is unknown
		} else{
			SetTargetFPS(fps);
		}
    }
    ~Window() noexcept{
        CloseWindow();
    }

    void render(const World& world, std::span<const Entity> entities) const noexcept{
        BeginDrawing();
        ClearBackground(CLEAR_COLOR);  
        world.render();
        for(const auto& e : entities){
            e.render(); 
            DrawText(TextFormat("Mode: %s", e.debug_state.data()), e.position.x+10, e.position.y+10, FONT_SIZE, DARKGRAY);
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
    std::vector<Entity> entities(1);
    World world;
    DemoTree tree;
    while(!window.should_close()){
        float deltaTime = GetFrameTime();
        if(IsKeyPressed(KEY_SPACE)) isPaused = !isPaused;
        if(IsKeyPressed(KEY_F)) world.wolfActive = !world.wolfActive;

        if(!isPaused){
            world.update(deltaTime);
            for(auto& e : entities){
                update_entity(e, tree, world, deltaTime);
            }
        }
        window.render(world, entities);
    }
    return 0;
}