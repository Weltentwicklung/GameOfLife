#include "seed_generator.h"
#include "grid_seed.h"
#include <cstdlib>
#include <ctime>

GridSeed generate_grid(int width, int height) {
	GridSeed grid_seed;
	grid_seed.grid.resize(width*height*4);

	return grid_seed;
}

GridSeed generate_random_seed(int width, int height, int percent_alive) {
	GridSeed grid_seed = generate_grid(width, height);

	// Fill the grid
	std::srand(std::time(nullptr));
	for (size_t i = 0; i < grid_seed.grid.size(); i += 4) {
		if((std::rand() % 100 + 1) <= percent_alive) {
			grid_seed.grid[i] = 1;
		}
		else {
			grid_seed.grid[i] = 0;
		}
		
	}

	return grid_seed;
	
}
