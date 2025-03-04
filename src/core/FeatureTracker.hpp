#pragma once

#include <vector>
#include <cstdint>

namespace cv {
	template<typename _Tp> class Point_;
	typedef Point_<float> Point2f;
}

class FeatureTracker {
	public:
		static std::vector<std::vector<float>> TrackFeatures(const std::vector<uint32_t*>& imageSequence, std::vector<cv::Point2f>& points, std::vector<std::vector<cv::Point2f>>& trackedPoints, int width, int height);
};
