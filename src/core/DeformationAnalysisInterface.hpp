#pragma once

#include <vector>
#include <cstdint>

#include <opencv2/opencv.hpp>

struct tile {
	cv::Mat data; // tile data
	cv::Point tl; // top-left corner of tile in original image
};

class DeformationAnalysisInterface {
	public:
		static bool RunModel(std::vector<uint32_t *> &images, int width, int height, const int tile_size, const int overlap, std::vector<tile>& tiles);
		static bool TestModelCPPFlow(std::vector<uint32_t*>& images, int width, int height, const int tile_size, const int overlap, std::vector<tile>& output_tiles);
};
