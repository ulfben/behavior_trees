#pragma once
#include "behavior-tree.hpp"
#include "steering.hpp"

// --- Leaf Functions ---
// these are either conditions for the entity to check, or actions it needs to take
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
    entity.debug_state = "FLEE";
    entity.acceleration += steer_flee(entity, ctx.world.wolf_pos, Entity::max_speed);
    entity.acceleration += steer_drag(entity);
    return Status::Running;
}

static Status MoveToCorner(Context& ctx, float) noexcept{
    auto& entity = ctx.self;
    entity.debug_state = "PATROL";
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

static Status AdvanceCorner(Context& ctx, float) noexcept{
    auto& entity = ctx.self;
    const auto count = (int) ctx.world.waypoints.size();
    entity.waypoint_index = (entity.waypoint_index + 1) % count;
    return Status::Success;
}

static Status DoSeekFood(Context& ctx, float) noexcept{
    auto& entity = ctx.self;
    entity.debug_state = "SEEK FOOD";
    entity.acceleration = ZERO;
    entity.acceleration += steer_seek(entity, ctx.world.food_pos, Entity::max_speed * 0.7f);
    entity.acceleration += steer_drag(entity);
    const float dist = Vector2Distance(entity.position, ctx.world.food_pos);
    if(dist < World::food_radius){
        entity.hunger = random_range(0.0f, 0.12f);
        entity.isHungry = false;
        return Status::Success;
    }
    return Status::Running;
}

//let's assemble a behavior tree :D 

struct DemoTree final{
    // threat branch
    Leaf threat{ThreatNearby};
    Leaf flee{DoFlee};
    Sequence fleeSeq{&threat, &flee};

    // patrol branch
    Leaf moveToCorner{MoveToCorner};
    Leaf advanceCorner{AdvanceCorner};
    MemorySequence patrolSeq{0, {&moveToCorner, &advanceCorner}};
    RepeatForever patrolLoop{&patrolSeq};

    // hunger branch
    Leaf hungry{CheckHunger};
    Leaf seekFood{DoSeekFood};
    Sequence foodSeq{&hungry, &seekFood};

    //this brain can: avoid threats, patrol waypoints, and find food when hungry.
    Selector root{&fleeSeq, &foodSeq, &patrolLoop};
    EntityBrain brain{&root};
};