/**
 * Behavior Trees Demo
 * -------------
 * This code was written for educational purposes.
 * Original repository: https://github.com/ulfben/behavior_trees/
 *
 * Copyright (c) 2026, Ulf Benjaminsson
 */
#include "common.hpp"
#include "entity.hpp"
#include "world.hpp"

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

struct RepeatForever final : Node{
	Node* child{};
	explicit RepeatForever(Node* c) : child(c){}

	Status tick(Context& ctx, float dt) const noexcept override{
		std::ignore = child->tick(ctx, dt);
		return Status::Running;
	}
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

static Status CheckHunger(Context& ctx, float) noexcept{
	// Hysteresis: enter hungry at 0.65, exit hungry at 0.45 
	if(!ctx.self.isHungry && ctx.self.hunger > 0.65f){
		ctx.self.isHungry = true;
	}if(ctx.self.isHungry && ctx.self.hunger < 0.45f){
		ctx.self.isHungry = false;
	}
	return ctx.self.isHungry ? Status::Success : Status::Failure;
}

static Status DoFlee(Context& ctx, float) noexcept{
	ctx.self.debug_state = "FLEE";	
	ctx.self.acceleration += steer_flee(ctx.self, ctx.world.wolfPos, Entity::max_speed);
	ctx.self.acceleration += steer_drag(ctx.self);
	return Status::Running;
}

static Status MoveToCorner(Context& ctx, float) noexcept{
	ctx.self.debug_state = "PATROL";
	const Vector2 target = ctx.world.waypoints[ctx.self.waypoint_index];
	const float dist = Vector2Distance(ctx.self.position, target);

	ctx.self.acceleration = ZERO;
	ctx.self.acceleration += steer_seek(ctx.self, target, Entity::max_speed * 0.65f);
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

static Status DoSeekFood(Context& ctx, float) noexcept{
	ctx.self.debug_state = "SEEK FOOD";	
	ctx.self.acceleration = ZERO;
	ctx.self.acceleration += steer_seek(ctx.self, ctx.world.foodPos, Entity::max_speed * 0.7f);
	ctx.self.acceleration += steer_drag(ctx.self);
	const float dist = Vector2Distance(ctx.self.position, ctx.world.foodPos);
	if(dist < 16.0f){
		ctx.self.hunger = 0.0f; // ate - now full 
		ctx.self.isHungry = false; // optional: immediate exit 
		return Status::Success;
	}
	return Status::Running;
}

static Status AlwaysRunning(Context&, float) noexcept{
	return Status::Running;
}

struct DemoTree final{
	//threat branch:
	Leaf threat{ThreatNearby};
	Leaf flee{DoFlee};
	Sequence fleeSeq{&threat, &flee};

	//patrol branch:
	Leaf moveToCorner{MoveToCorner};
	Leaf advanceCorner{AdvanceCorner};
	MemorySequence patrolSeq{0, {&moveToCorner, &advanceCorner}};
	RepeatForever patrolLoop{&patrolSeq};

	//hunger branch:
	Leaf hungry{CheckHunger};
	Leaf seekFood{DoSeekFood};
	Sequence foodSeq{&hungry, &seekFood};
	
	//this brain can: avoid threats, patrol waypoints, and find food when hungry.
	Selector root{&fleeSeq, &foodSeq, &patrolLoop}; 
	EntityBrain brain{&root};
};

// free function update, to avoid circular dependency problems
static void update_entity(Entity& e, DemoTree& tree, World& world, float dt) noexcept{
	Context ctx{e, world};

	// Tick behavior tree
	std::ignore = tree.brain.tick(ctx, dt);	

	// Boids-style integration (steering -> velocity -> position)
	e.velocity += e.acceleration * dt;
	e.velocity = Vector2ClampValue(e.velocity, Entity::min_speed, Entity::max_speed);
	e.position += e.velocity * dt;
	e.position = wrap(e.position);
	e.acceleration = ZERO; // clear for next tick to force leaves to set it

	
	e.hunger = std::clamp(e.hunger + Entity::hunger_per_second * dt, 0.0f, 1.0f);
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
			if(e.debug_state == "SEEK FOOD"){
				DrawLineV(e.position, world.foodPos, Fade(DARKGREEN, 0.5f));
			} else if(e.debug_state == "PATROL"){
				DrawText(TextFormat("WP: %d", e.waypoint_index), (int) p.x, (int) p.y + FONT_SIZE, FONT_SIZE, DARKGRAY);			
				DrawLineV(e.position, world.waypoints[e.waypoint_index], Fade(DARKGREEN, 0.5f));
			}			
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
	std::vector<Entity> entities(3);
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
