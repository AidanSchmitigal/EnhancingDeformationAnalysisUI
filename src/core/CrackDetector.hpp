#pragma once

#include <vector>
#include <cstdint>

class CrackDetector {
public:
		static void detectCracks(std::vector<uint32_t*>& frames, int width, int height);
};