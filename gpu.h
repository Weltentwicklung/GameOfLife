#pragma once
#include "settings.h"
#include "grid_seed.h"


bool initialize_gpu(Settings& settings, GridSeed& grid_seed);
void gpu_generation_loop();  // runs continuously in its own thread
void set_zoom(Settings& settings);
void set_offset(Settings& settings);
void cleanup_gpu();
