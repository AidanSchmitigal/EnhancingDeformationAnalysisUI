#pragma once

#include <vector>
#include <cstdint>

#include <opencv2/opencv.hpp>

class CrackDetector {
public:
	static std::vector<std::vector<std::vector<cv::Point>>> DetectCracks(const std::vector<uint32_t*>& images, int width, int height, int crack_darkness = 40, int fill_threshold = 2, int sharpness = 50, int resolution = 3, int amount = 1);
};
