#pragma once

#include <vector>
#include <cstdint>

class CrackDetector {
public:
		static void DetectCracks(std::vector<uint32_t*>& frames, int width, int height);
};
