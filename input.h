#pragma once

#include "settings.h"

bool initialize_input();
void process_input(bool& running, Settings& settings);
void cleanup_input();
