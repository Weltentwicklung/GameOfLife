#pragma once

#include "settings.h"
#include "constants.h"
#include "gpu.h"
#include <atomic>

bool initialize_input();
void process_input(std::atomic<bool>& running, Settings& settings);
void cleanup_input();
