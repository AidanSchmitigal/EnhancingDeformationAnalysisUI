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
		static bool TestModel(std::vector<uint32_t *> &images, int width, int height, const int tile_size, const int overlap, std::vector<tile>& tiles);
	private:
		// Structure to hold tile information
		struct ImageTile {
			cv::Mat data;       // The processed tile
			cv::Point position; // Top-left position in original image
			cv::Size size;      // Size of the tile
		};
		static std::vector<ImageTile> splitImageIntoTiles(const cv::Mat &image, const int tile_size, const int overlap);
		static cv::Mat reconstructImageFromTiles(const std::vector<ImageTile> &tiles, cv::Size size, const int overlap);
};
