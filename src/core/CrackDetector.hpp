#pragma once

#include <vector>
#include <cstdint>

class CrackDetector {
public:
	static void DetectCracks(const std::vector<uint32_t*>& images, int width, int height, int crack_darkness = 40, int fill_threshold = 2, int sharpness = 50, int resolution = 3, int amount = 1);
};
