#pragma once
#include <vector>
#include <array>
extern std::vector<std::array<int,3>> mockColors;
inline void neopixelWrite(int, int r, int g, int b) { mockColors.push_back({r,g,b}); }
