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
	std::thread generation_thread;

	std::atomic<bool> running = true;

	// maybe setting up everything outside of the loop?
	// create a seed, for now std seed
	GridSeed initial_seed = generate_random_seed(GRID_W, GRID_H, SEED_DENSITY);
	Settings settings;
	settings.offset_y = 0;
	settings.offset_x = 0;
	settings.pixels_per_cell = 1;
	settings.generation_loop_ms = 200;
	
	if(!initialize_input()) return 1;
	if(!initialize_gpu(initial_seed, initial_settings)) {
	    cleanup_input();
	    return 1;
	}

	input_thread = std::thread([&]) {
		while(running){
			//set loop timer
			auto loop_start = std::chrono::steady_clock::now();

		//check input and or checking gpu
			process_input(running, settings);

		// calculate remaining time and execute sleep for it
			auto elapsed_time = std::chrono::steady_clock::now() - loop_start;
			auto sleep_time = std::chrono::milliseconds(MAIN_LOOP_MS) - elapsed_time;

			if (sleep_time > std::chrono::milliseconds(0)){
				std::this_thread::sleep_for(sleep_time);
			}
		}	
	}

	generation_thread = std::thread([&]) {
		while(running){
			//set loop timer
			auto loop_start = std::chrono::steady_clock::now();
	
			gpu_generation_loop();

			// calculate remaining time and execute sleep for it
			auto elapsed_time = std::chrono::steady_clock::now() - loop_start;
			g_settings_mutex.lock();
			auto sleep_time = std::chrono::milliseconds(settings.generation_loop_ms) - elapsed_time;
			g_settings_mutex.unlock();	
			if (sleep_time > std::chrono::milliseconds(0)){
				std::this_thread::sleep_for(sleep_time);
			}
		}
	}

	input_thread.join();
	generation_thread.join();
	cleanup_gpu();
	cleanup_input();
	
}
