#pragma once

#include <vector>
#include <cstdint>

namespace cv {
	template<typename _Tp> class Point_;
	typedef Point_<int> Point2i;
	typedef Point2i Point;
}

class CrackDetector {
public:
	static std::vector<std::vector<std::vector<cv::Point>>> DetectCracks(const std::vector<uint32_t*>& images, int width, int height, int crack_darkness = 40, int fill_threshold = 2, int sharpness = 50, int resolution = 3, int amount = 1);
};
