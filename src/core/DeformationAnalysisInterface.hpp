#pragma once

#include <cstdint>
#include <functional>
#include <future>
#include <vector>

#include <utils.h>

#include <opencv2/opencv.hpp>

class DeformationAnalysisInterface {
      public:
	// Synchronous model execution
	static bool RunModel(std::vector<uint32_t *> &images, int width, int height, std::vector<Tile> &tiles,
			     const TileConfig &tile_config);

	// Asynchronous model execution with callback
	static std::future<bool> RunModelAsync(
	    std::vector<uint32_t *> &images, int width, int height, std::vector<Tile> &tiles,
	    const TileConfig &tile_config, std::function<void(bool)> callback = [](bool) {});

	// Batch processing
	enum class BatchProcessMode {
		Consecutive,	// Process each consecutive pair (0,1), (1,2), etc.
		ReferenceFrame, // Process with reference frame (0,1), (0,2), etc.
		Custom		// Process specified pairs
	};

	struct BatchProcessingParams {
		BatchProcessMode mode = BatchProcessMode::Consecutive;
		int referenceFrameIndex = 0;		     // For ReferenceFrame mode
		std::vector<std::pair<int, int>> framePairs; // For Custom mode
	};

	// Batch process multiple frames asynchronously
	static std::future<bool> BatchProcessAsync(
	    std::vector<uint32_t *> &images, int width, int height, BatchProcessingParams params,
	    std::vector<Tile> &tiles, const TileConfig &tile_config, std::function<void(bool)> callback = [](bool) {});

	static bool TestModelCPPFlow(std::vector<uint32_t *> &images, int width, int height, const int tile_size,
				     const int overlap, std::vector<Tile> &output_tiles);

	static bool IsProcessing() { return m_processing; }
	static float GetProgress() { return m_progress; }

      private:
	static bool m_processing;
	static float m_progress;
};
