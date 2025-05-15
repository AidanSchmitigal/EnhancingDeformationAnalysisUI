#pragma once

#include <vector>
#include <cstdint>

#include <utils.h>

#include <opencv2/opencv.hpp>

class DeformationAnalysisInterface {
	public:
		static bool RunModel(std::vector<uint32_t *> &images, int width, int height, const int tile_size, const int overlap, std::vector<Tile>& tiles);
		static bool TestModelCPPFlow(std::vector<uint32_t*>& images, int width, int height, const int tile_size, const int overlap, std::vector<Tile>& output_tiles);

		static bool IsProcessing() {
			return m_processing;
		}
	private:
		static bool m_processing;
};
