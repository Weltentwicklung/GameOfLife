#include <iostream>
#include <chrono>
#include <thread>
#include "constants.h"
#include "grid_seed.h"
#include "seed_generator.h"
#include "settings.h"
#include "gpu.h"


int main(){
	bool running = true;


	// maybe setting up everything outside of the loop?
	// create a seed, for now std seed
	GridSeed initial_seed = generate_random_seed(1920, 1090, 25); // would that match my cellphones (samsung galaxy s 8) resolution?
	Settings initial_settings;
	initial_settings.offset_y = 0;
	initial_settings.offset_x = 0;
	initial_settings.pixel_per_cell = 1;
	initial_settings.generation_loop_ms = 200ms;

	
	start_gpu(initial_seed, initial_settings);

	while(running){
		//set loop timer
		auto loop_start = std::chrono::steady_clock::now();


		//check input and or checking gpu
		//

		// calculate remaining time and execute sleep for it
		auto elapsed_time = std::chrono::steady_clock::now() - loop_start;
		auto sleep_time = std::chrono::milliseconds(MAIN_LOOP_MS) - elapsed_time;

		if (sleep_time > std::chrono::milliseconds(0)){
			std::this_thread::sleep_for(sleep_time);
		}
		
	}

	return 0;
}
