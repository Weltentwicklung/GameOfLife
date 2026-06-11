#pragma once

#include <mutex>

struct Settings {
	int offset_y;
	int offset_x;
	int pixel_per_cell;
	int generation_loop_ms;
}

extern std::mutex g_settings_mutex;
