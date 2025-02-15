#pragma once

#include <vector>
#include <cstdint>

struct TrackingPoint {
    float x;
    float y;
};
class FeatureTracker {
public:
	static std::vector<std::pair<TrackingPoint, TrackingPoint>> TrackFeatures(const std::vector<uint32_t*>& imageSequence, int width, int height, TrackingPoint point1, TrackingPoint point2);
};
