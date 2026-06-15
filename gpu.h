#pragma once
#include "settings.h"
#include "grid_seed.h"
#include "constants.h"


bool initialize_gpu(Settings& settings, GridSeed& grid_seed);
void gpu_generation_loop(Settings& settings);  // runs continuously in its own thread
void cleanup_gpu();
