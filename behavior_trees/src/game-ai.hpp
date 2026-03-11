#pragma once
#include "behavior-tree.hpp"
#include "steering.hpp"
using namespace std::string_view_literals;

// --- Leaf Functions ---
// these are either conditions for the entity to check, or actions it needs to take
// these functions are how our leaf nodes actually interacts with the world
static Status ThreatNearby(Context& ctx, float) noexcept{    
    if(!ctx.world.wolf_active) return Status::Failure;
    auto& entity = ctx.self;
    const float dist = Vector2Distance(entity.position, ctx.world.wolf_pos);
    return (dist < 180.0f) ? Status::Success : Status::Failure;
}

static Status CheckHunger(Context& ctx, float) noexcept{
    auto& entity = ctx.self;
    if(!entity.isHungry && entity.hunger > 0.95f){
        entity.isHungry = true;
    }
    if(entity.isHungry && entity.hunger < 0.05f){
        entity.isHungry = false;
    }
    return entity.isHungry ? Status::Success : Status::Failure;
}

static Status DoFlee(Context& ctx, float) noexcept{
    auto& entity = ctx.self;
    entity.debug_state = "FLEE"sv;
    entity.acceleration += steer_flee(entity, ctx.world.wolf_pos, Entity::max_speed);
    entity.acceleration += steer_drag(entity);
    return Status::Running;
}

static Status MoveToWaypoint(Context& ctx, float) noexcept{
    auto& entity = ctx.self;
    entity.debug_state = "PATROL"sv;
    const Vector2 target = ctx.world.waypoints[entity.waypoint_index];
    const float dist = Vector2Distance(entity.position, target);

    entity.acceleration = ZERO;
    entity.acceleration += steer_seek(entity, target, Entity::max_speed * 0.65f);
    entity.acceleration += steer_drag(entity);

    if(dist <= World::waypoint_radius){
        return Status::Success;
    }
    return Status::Running;
}

static Status AdvanceWaypoint(Context& ctx, float) noexcept{
    auto& entity = ctx.self;
    const auto count = (int) ctx.world.waypoints.size();
    entity.waypoint_index = (entity.waypoint_index + 1) % count;
    return Status::Success;
}

static Status DoSeekFood(Context& ctx, float) noexcept{
    auto& entity = ctx.self;
    entity.debug_state = "SEEK FOOD"sv;
    entity.acceleration = ZERO;
    entity.acceleration += steer_seek(entity, ctx.world.food_pos, Entity::max_speed * 0.7f);
    entity.acceleration += steer_drag(entity);
    const float dist = Vector2Distance(entity.position, ctx.world.food_pos);
    if(dist < World::food_radius){
        entity.hunger = random_range(0.0f, 0.12f);
        entity.isHungry = false;
        ctx.world.respawn_food();
        return Status::Success;
    }
    return Status::Running;
}

//let's assemble a behavior tree :D 

struct DemoTree final{
    // threat branch
    Leaf threat{ThreatNearby, "\t\tIs Threat Nearby?"sv};
    Leaf flee{DoFlee, "\t\tFlee"sv};
    Sequence fleeSeq{{ &threat, &flee }, "\t->Avoid Threats"sv};

    // hunger branch
    Leaf hungry{CheckHunger, "\t\tAre we hungry?"sv};
    Leaf seekFood{DoSeekFood, "\t\tSeek Food"sv};
    Sequence foodSeq{{&hungry, &seekFood}, "\t->Don't Starve"sv};   

    // patrol branch
    Leaf moveToWaypoint{MoveToWaypoint, "\t\t\tMove To Waypoint"sv};
    Leaf advanceWaypoint{AdvanceWaypoint, "\t\t\tPick Next Waypoint"sv};
    MemorySequence patrolSeq{0, {&moveToWaypoint, &advanceWaypoint}, "\t\tPatrol Waypoints"sv};
    RepeatForever patrolLoop{&patrolSeq, "\t->Repeat Forever"sv};     

    //this brain can: avoid threats, patrol waypoints, and find food when hungry.
    Selector root{{&fleeSeq, &foodSeq, &patrolLoop}, "Root (DemoTree)"sv};
    EntityBrain brain{&root};
};