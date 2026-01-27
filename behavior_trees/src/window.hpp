#pragma once
#include "common.hpp"

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
	bool should_close() const noexcept{
		return WindowShouldClose() || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q);
	}
};