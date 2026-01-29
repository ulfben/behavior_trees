/**
 * Behavior Trees Demo
 * -------------
 * This code was written for educational purposes.
 * Original repository: https://github.com/ulfben/behavior_trees/
 *
 * Copyright (c) 2026, Ulf Benjaminsson
 */
#include "common.hpp"
#include "window.hpp"
#include "entity.hpp"
#include "world.hpp"
#include "steering.hpp"
#include "behavior-tree.hpp"
#include "game-ai.hpp"

static void update(World& world, const DemoTree& tree, std::span<Entity> entities, float dt) noexcept{
	world.update(dt);
	for(auto& e : entities){
		Context ctx{e, world};
		std::ignore = tree.brain.tick(ctx, dt);
		e.update(dt);
	}
}

static void render(const World& world, std::span<const Entity> entities) noexcept{
	BeginDrawing();
	ClearBackground(CLEAR_COLOR);
	world.render();
	for(const auto& e : entities){
		e.render();
		Vector2 p = {e.position.x + 10.0f, e.position.y + 10.0f};
		DrawText(TextFormat("Mode: %s", e.debug_state.data()), p.x, p.y, FONT_SIZE, DARKGRAY);
		if(e.debug_state == "SEEK FOOD"){
			DrawLineV(e.position, world.food_pos, Fade(DARKGREEN, 0.5f));
		} else if(e.debug_state == "PATROL"){
			DrawText(TextFormat("WP: %d", e.waypoint_index), p.x, p.y + FONT_SIZE, FONT_SIZE, DARKGRAY);
			DrawLineV(e.position, world.waypoints[e.waypoint_index], Fade(DARKGREEN, 0.5f));
		}
	}
	DrawText("Press SPACE to pause/unpause", 10, STAGE_HEIGHT - FONT_SIZE, FONT_SIZE, DARKGRAY);
	DrawFPS(10, STAGE_HEIGHT - FONT_SIZE * 2);
	EndDrawing();
}

int main(){
	auto window = Window(STAGE_WIDTH, STAGE_HEIGHT, "Behavior Tree Demo");
	bool isPaused = false;
	std::vector<Entity> entities(1);
	World world;
	DemoTree tree;
	while(!window.should_close()){
		float deltaTime = GetFrameTime();		
		if(IsKeyPressed(KEY_SPACE)){ 
			isPaused = !isPaused; 
		}
		if(IsKeyPressed(KEY_F)){ 
			world.wolf_active = !world.wolf_active; 
		}
		if(!isPaused){
			update(world, tree, entities, deltaTime);
		}
		render(world, entities);
	}
	return 0;
}