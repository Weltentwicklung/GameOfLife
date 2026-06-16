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
    std::srand(std::time(nullptr));
    for(size_t i = 0; i < grid_seed.grid.size(); i += 4) {
        if((std::rand() % 100 + 1) <= percent_alive) {
            grid_seed.grid[i] = 1;
        }
        else {
            grid_seed.grid[i] = 0;
        }
    }
    return grid_seed;
}


GridSeed generate_template_seed(int width, int height, const std::vector<std::pair<int,int>> &cells) {
	GridSeed grid_seed = generate_grid(width, height);

    // find bounding box
    int max_x = 0, max_y = 0;
    for(const auto& cell : cells) {
        if(cell.first  > max_x) max_x = cell.first;
        if(cell.second > max_y) max_y = cell.second;
    }

    // center offset
    int offset_x = (width  - (max_x + 1)) / 2;
    int offset_y = (height - (max_y + 1)) / 2;

    // place cells
    for(const auto& cell : cells) {
        int x = cell.first  + offset_x;
        int y = cell.second + offset_y;
        int index = (y * width + x) * 4;
        grid_seed.grid[index] = 1;
    }
	
    return grid_seed;
}
