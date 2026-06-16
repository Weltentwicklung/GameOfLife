#pragma once
#include <vector>
#include <utility>

enum class PatternType {
    GOSPER_GLIDER_GUN,
    MAX,
    SNARK_LOOP,
    P80_HWSS_GUN,
    DART_SYNTH,
    CRYSTALLIZATION_OSCILLATOR
};

struct Pattern {
    const char* name;
    const char* author;
    std::vector<std::pair<int,int>> cells;
};

extern const Pattern PATTERNS[];
extern const int PATTERN_COUNT;
