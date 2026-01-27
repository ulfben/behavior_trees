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
#include <cassert>
#include <string_view>
#include <span>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <array>

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
    static constexpr float drag = 0.01f;
    static constexpr float seek_weight = 1.0f;
    static constexpr float flee_weight = 1.2f;
    static constexpr float wander_weight = 1.0f;
    static constexpr float wander_radius = 60.0f;
    static constexpr float wander_distance = 80.0f;
    static constexpr float wander_jitter = 4.0f;   // radians/sec-ish when multiplied by dt
    
    // Patrol mission
    int waypoint_index = 0;
    static constexpr float waypoint_radius = 18.0f;    
    std::array<int, 8> bt_mem{}; // Behavior-tree memory (one slot is enough for this demo)
        
    std::string_view debug_state = "None";    
    float wander_angle = random_range(0.0f, 2.0f * PI);
    Vector2 position = random_range(ZERO, STAGE_SIZE);
    Vector2 acceleration = ZERO;
    Vector2 velocity = vector_from_angle(wander_angle, MIN_SPEED);
    float hunger = random_range(0.0f, 1.0f); // 0 = full, 1 = starving
    bool isHungry = false;

    void render() const noexcept{
        Vector2 local_x = (Vector2Length(velocity) != 0) ? Vector2Normalize(velocity) : Vector2{1, 0};
        Vector2 local_y = {-local_x.y, local_x.x};
        float L = ENTITY_SIZE;
        float H = ENTITY_SIZE;
        Vector2 tip = position + (local_x * L * 1.4f);
        Vector2 left = position - (local_x * L) + (local_y * H);
        Vector2 right = position - (local_x * L) - (local_y * H);
        DrawTriangle(tip, right, left, GREEN);
        DrawLineV(position, position + velocity, BLUE);
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

// MemorySequence - remembers which child was running for this entity.
struct MemorySequence final : Node{
    std::vector<Node*> children;
    int mem_slot = 0;

    MemorySequence(int slot, std::initializer_list<Node*> xs)
        : children(xs), mem_slot(slot){}

    Status tick(Context& ctx, float dt) const noexcept override;
};

// Leaf node: either a condition or an action, supplied as a function pointer.
// Could use std::function or lambdas, but we opt for plain function pointer 
// to enforce that leaf nodes are stateless; all behavior state lives in Context
using LeafFn = Status(*)(Context&, float) noexcept;

struct Leaf final : Node{
    LeafFn fn{};
    explicit Leaf(LeafFn f) : fn(f){}
    Status tick(Context& ctx, float dt) const noexcept override{ return fn(ctx, dt); }
};

static Vector2 wrap(Vector2 p) noexcept{
    if(p.x < ENTITY_SIZE) p.x += STAGE_WIDTH;
    if(p.y < ENTITY_SIZE) p.y += STAGE_HEIGHT;
    if(p.x >= STAGE_WIDTH) p.x -= STAGE_WIDTH + ENTITY_SIZE;
    if(p.y >= STAGE_HEIGHT) p.y -= STAGE_HEIGHT + ENTITY_SIZE;
    return p;
}

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
        wolfPos.x = (STAGE_WIDTH * 0.5f) + std::cos(t * 0.7f) * (STAGE_WIDTH * 0.28f);
        wolfPos.y = (STAGE_HEIGHT * 0.5f) + std::sin(t * 1.1f) * (STAGE_HEIGHT * 0.22f);
    }

    void render() const noexcept{
        for(int i = 0; i < 4; ++i){
            DrawCircleV(waypoints[i], 6.0f, DARKGREEN);
            DrawText(TextFormat("%d", i), (int) waypoints[i].x + 8, (int) waypoints[i].y - 8, FONT_SIZE, DARKGREEN);
        }
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

Status MemorySequence::tick(Context& ctx, float dt) const noexcept{
    int& i = ctx.self.bt_mem[mem_slot];

    while(i < (int) children.size()){
        const Status s = children[i]->tick(ctx, dt);

        if(s == Status::Running) return Status::Running;
        if(s == Status::Failure){ i = 0; return Status::Failure; }

        ++i; // child succeeded - advance
    }

    i = 0;
    return Status::Success;
}

//some useful steering behaviors to let entities navigate the world
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


// Demo leaves (conditions/actions)
static Status ThreatNearby(Context& ctx, float) noexcept{
    if(!ctx.world.wolfActive) return Status::Failure;
    const float dist = Vector2Distance(ctx.self.position, ctx.world.wolfPos);
    return (dist < 180.0f) ? Status::Success : Status::Failure;
}

static Status DoFlee(Context& ctx, float) noexcept{
    ctx.self.debug_state = "FLEE";
    ctx.self.acceleration = ZERO;
    ctx.self.acceleration += steer_flee(ctx.self, ctx.world.wolfPos, Entity::MAX_SPEED);
    ctx.self.acceleration += steer_drag(ctx.self);
    return Status::Running;
}

static Status MoveToCorner(Context& ctx, float) noexcept{
    ctx.self.debug_state = "PATROL";
    const Vector2 target = ctx.world.waypoints[ctx.self.waypoint_index];
    const float dist = Vector2Distance(ctx.self.position, target);

    ctx.self.acceleration = ZERO;
    ctx.self.acceleration += steer_seek(ctx.self, target, Entity::MAX_SPEED * 0.65f);
    ctx.self.acceleration += steer_drag(ctx.self);

    if(dist <= Entity::waypoint_radius){
        return Status::Success;
    }
    return Status::Running;
}

static Status AdvanceCorner(Context& ctx, float) noexcept{
    ctx.self.waypoint_index = (ctx.self.waypoint_index + 1) % 4;
    return Status::Success;
}

static Status AlwaysRunning(Context&, float) noexcept{
    return Status::Running;
}

struct DemoTree final{
    Leaf threat{ThreatNearby};
    Leaf flee{DoFlee};

    Leaf moveToCorner{MoveToCorner};
    Leaf advanceCorner{AdvanceCorner};
    Leaf runForever{AlwaysRunning};

    Sequence fleeSeq{&threat, &flee};

    MemorySequence patrolSeq{0, {&moveToCorner, &advanceCorner, &runForever}};

    Selector root{&patrolSeq};

    EntityBrain brain{&root};
};

// free function update, to avoid circular dependency problems
static void update_entity(Entity& e, DemoTree& tree, World& world, float dt) noexcept{
    Context ctx{e, world};
    // Tick behavior tree
    std::ignore = tree.brain.tick(ctx, dt);

    // Boids-style integration (steering -> velocity -> position)
    e.velocity += e.acceleration * dt;
    e.velocity = Vector2ClampValue(e.velocity, Entity::MIN_SPEED, Entity::MAX_SPEED);
    e.position += e.velocity * dt;
    e.position = wrap(e.position);    
    e.acceleration = ZERO; // clear for next tick to force leaves to set it
}

struct Window final{
    Window(int width, int height, std::string_view title, int fps = 0){
        InitWindow(width, height, title.data());
        if(!IsWindowReady()){
            throw std::runtime_error("Unable to create Raylib window. Check settings?");
        }
        if(fps < 1){
            int hz = GetMonitorRefreshRate(GetCurrentMonitor());
            SetTargetFPS(hz > 0 ? hz : 60);
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
            Vector2 p = {e.position.x + 10.0f, e.position.y + 10.0f};
            DrawText(TextFormat("Mode: %s", e.debug_state.data()), (int) p.x, (int) p.y, FONT_SIZE, DARKGRAY);
            DrawText(TextFormat("WP: %d", e.waypoint_index), (int) p.x, (int) p.y + FONT_SIZE, FONT_SIZE, DARKGRAY);
            DrawText(TextFormat("mem0: %d", e.bt_mem[0]), (int) p.x, (int) p.y + 2 * FONT_SIZE, FONT_SIZE, DARKGRAY);
            DrawLineV(e.position, world.waypoints[e.waypoint_index], Fade(DARKGREEN, 0.4f));
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
    auto window = Window(STAGE_WIDTH, STAGE_HEIGHT, "Behavior Tree Demo - Patrol Corners");
    bool isPaused = false;
    std::vector<Entity> entities(2);
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
