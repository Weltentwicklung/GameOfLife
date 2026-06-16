#pragma once
#include "grid_seed.h"
#include "patterns.h"

GridSeed generate_grid(int width, int height);
GridSeed generate_random_seed(int width, int height, int percent_alive);
GridSeed generate_template_seed(int width, int height, const std::vector<std::pair<int,int>>& cells);
