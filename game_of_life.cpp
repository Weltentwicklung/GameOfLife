#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include "constants.h"
#include "grid_seed.h"
#include "seed_generator.h"
#include "settings.h"
#include "gpu.h"


int main(){

	std::thread input_thread;
	std::thread gpu_thread;

	bool running = true;

	// maybe setting up everything outside of the loop?
	// create a seed, for now std seed
	GridSeed initial_seed = generate_random_seed(GRID_W, GRID_H, SEED_DENSITY);
	Settings initial_settings;
	initial_settings.offset_y = 0;
	initial_settings.offset_x = 0;
	initial_settings.pixel_per_cell = 1;
	initial_settings.generation_loop_ms = 200;
	
	start_gpu(initial_seed, initial_settings);

	input_thread = std::thread([]()) {
		while(running){
			//set loop timer
			auto loop_start = std::chrono::steady_clock::now();

		//check input and or checking gpu
			process_input(&running, settings);

		// calculate remaining time and execute sleep for it
			auto elapsed_time = std::chrono::steady_clock::now() - loop_start;
			auto sleep_time = std::chrono::milliseconds(MAIN_LOOP_MS) - elapsed_time;

			if (sleep_time > std::chrono::milliseconds(0)){
				std::this_thread::sleep_for(sleep_time);
			}
		}

		return 0; // would return 0 be right here ??? actually i think it's wrong
		// we dont wanna terminate main here, we need to join and destroy both threads properly
		// so we could use a input_thread.join() here or just leave and use it outside...
			
				
	}

	generation_thread = std::thread([]()) {
		
	}

	input_thread.join();
	generation_thread.join();
	cleanup_gpu();
	cleanup_input();
	
}
