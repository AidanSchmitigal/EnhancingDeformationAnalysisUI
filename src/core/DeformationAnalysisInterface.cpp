#include "DeformationAnalysisInterface.hpp"

#include <torch/types.h>
#include <utils.h>

#include <opencv2/opencv.hpp>

#ifdef UI_INCLUDE_TENSORFLOW
#include <cppflow/cppflow.h>
#endif

#include <functional>
#include <future>
#include <torch/script.h>
#include <torch/torch.h>

bool DeformationAnalysisInterface::m_processing = false;
float DeformationAnalysisInterface::m_progress = 0.0f;

bool DeformationAnalysisInterface::RunModel(std::vector<uint32_t *> &images, int width, int height,
					    std::vector<Tile> &output_tiles, const TileConfig &tile_config) {
	PROFILE_FUNCTION();

	m_processing = true;
	m_progress = 0.0f;

#ifdef UI_INCLUDE_PYTORCH
	auto to_tensor = [](const cv::Mat &m) {
		return torch::from_blob(m.data, {1, 1, 256, 256}, torch::kUInt8).to(torch::kFloat32).clone();
	};

	auto dev = torch::cuda::is_available() ? torch::kCUDA : torch::kCPU;
	// input: 2 images, each 256x256 and need to be converted to 1x256x256
	// (1 channel) input: data format: float between [0, 255] output: 2
	// images, each 1x256x256 (total output batchx2x256x256) output: data
	// format: float between [-2, 2]
	auto model = torch::jit::load("assets/models/batch-m4-combo.pt");
	model.to(dev);
	model.eval();

	// Make sure we have at least 2 images to process
	if (images.size() < 2) {
		m_progress = 1.0f;
		m_processing = false;
		return false;
	}

	for (size_t i = 0; i < images.size() - 1; ++i) {
		// Update progress
		m_progress = (float)i / std::max(1, (int)images.size() - 1);

		// === INPUT FORMATTING ===
		cv::Mat image = cv::Mat(height, width, CV_8UC4, images[i]);
		cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
		auto tiles = Tiler::CreateTiles(image, tile_config);
		cv::Mat image2 = cv::Mat(height, width, CV_8UC4, images[i + 1]);
		cv::cvtColor(image2, image2, cv::COLOR_BGRA2GRAY);
		auto tiles2 = Tiler::CreateTiles(image2, tile_config);

		std::vector<Tile> outTiles;
		for (int k = 0; k < tiles.size(); ++k) {
			// === MODEL INFERENCE ===
			auto input = torch::cat({to_tensor(tiles[k].data), to_tensor(tiles2[k].data)}, 1).to(dev);

			auto out = model.forward({input}).toTensor().to(torch::kCPU); // out: torch::Tensor on cpu,
										      // shape 1,2,H,W

			// pull out three {H,W} uint8 planes
			auto t = out.squeeze(0);
			auto r = t.select(0, 0);
			auto g = t.select(0, 1);
			auto z = torch::zeros_like(r);

			// normalize & to uint8 in one go
			auto to_u8 = [&](torch::Tensor x) {
				return x
				    .add(2.0) // output is generally within [-2,
					      // 2] (it's odd)
				    .div(4.0)
				    .mul(255)
				    .clamp(0, 255)
				    .to(torch::kUInt8);
			};
			// two channels of actual info, one of zeros to complete
			// a 3-channel image
			auto ru8 = to_u8(r); // H,W
			auto gu8 = to_u8(g);
			auto zu8 = torch::zeros_like(ru8);

			// copy each into a cv::Mat and merge
			int H = ru8.size(0), W = ru8.size(1);
			cv::Mat mr(H, W, CV_8UC1, ru8.data_ptr<uint8_t>());
			cv::Mat mg(H, W, CV_8UC1, gu8.data_ptr<uint8_t>());
			cv::Mat mz(H, W, CV_8UC1, zu8.data_ptr<uint8_t>());

			std::vector<cv::Mat> chans = {mz, mg, mr};

			// merge the channels into one cv::Mat
			cv::Mat bgr;
			cv::merge(chans,
				  bgr); // b=0, g=chan1, r=chan0
					// convert to BGRA (add alpha channel)
			cv::cvtColor(bgr, bgr, cv::COLOR_BGR2BGRA);
			outTiles.push_back({bgr.clone(), tiles[k].position}); // own memory
			output_tiles.push_back(
			    {bgr.clone(), tiles[k].position, (int)i}); // own memory with source frame index
		}
		auto stitched = Tiler::StitchTiles(outTiles, tile_config, image.size());

		// copy back to the original image
		memcpy(images[i], stitched.data, width * height * sizeof(uint32_t));
	}

	m_progress = 1.0f;
	m_processing = false;
	return true;
#else  // UI_INCLUDE_PYTORCH
	m_processing = false;
	return false;
#endif // UI_INCLUDE_PYTORCH
}

// Asynchronous version of the model execution
std::future<bool> DeformationAnalysisInterface::RunModelAsync(std::vector<uint32_t *> &images, int width, int height,
							      std::vector<Tile> &tiles, const TileConfig &tile_config,
							      std::function<void(bool)> callback) {

	m_processing = true;
	m_progress = 0.0f;

	// Create copies of the vectors to avoid reference issues
	std::vector<uint32_t *> images_copy = images;

	// Creates a task that will be run asynchronously - we capture images by value (as a copy)
	auto task = [images_copy, width, height, &tiles, tile_config, callback]() mutable {
		bool result = RunModel(images_copy, width, height, tiles, tile_config);
		callback(result);
		return result;
	};

	return std::async(std::launch::async, task);
}

// Batch processing implementation
std::future<bool> DeformationAnalysisInterface::BatchProcessAsync(std::vector<uint32_t *> &images, int width,
								  int height, BatchProcessingParams params,
								  std::vector<Tile> &tiles,
								  const TileConfig &tile_config,
								  std::function<void(bool)> callback) {

	// Create a copy of the vector to avoid reference issues
	std::vector<uint32_t *> images_copy = images;

	auto task = [images_copy, width, height, params, &tiles, &tile_config, callback]() mutable {
		m_processing = true;
		m_progress = 0.0f;

		bool result = true;

#ifdef UI_INCLUDE_PYTORCH
		auto to_tensor = [](const cv::Mat &m) {
			return torch::from_blob(m.data, {1, 1, 256, 256}, torch::kUInt8).to(torch::kFloat32).clone();
		};

		auto dev = torch::cuda::is_available() ? torch::kCUDA : torch::kCPU;

		// Load model once for all batch operations
		auto model = torch::jit::load("assets/models/batch-m4-combo.pt");
		model.to(dev);
		model.eval();

		// Prepare frame pairs to process based on the batch mode
		std::vector<std::pair<int, int>> framePairs;

		// Make sure we have at least 2 images to process
		if (images_copy.size() < 2) {
			// Not enough images to process
			result = false;
			m_processing = false;
			m_progress = 1.0f;
			callback(result);
			return result;
		}

		if (params.mode == BatchProcessMode::Consecutive) {
			// Process consecutive pairs: (0,1), (1,2), etc.
			for (size_t i = 0; i < images_copy.size() - 1; i++) {
				framePairs.push_back({static_cast<int>(i), static_cast<int>(i + 1)});
			}
		} else if (params.mode == BatchProcessMode::ReferenceFrame) {
			// Use a reference frame as first frame: (ref,1), (ref,2), etc.
			int refIdx = params.referenceFrameIndex;
			if (refIdx < 0 || refIdx >= static_cast<int>(images_copy.size())) {
				refIdx = 0; // Default to first frame if out of bounds
			}

			for (size_t i = 0; i < images_copy.size(); i++) {
				if (static_cast<int>(i) != refIdx) {
					framePairs.push_back({refIdx, static_cast<int>(i)});
				}
			}
		} else if (params.mode == BatchProcessMode::Custom) {
			// Use custom specified frame pairs
			framePairs = params.framePairs;

			// Validate frame pairs
			for (auto it = framePairs.begin(); it != framePairs.end();) {
				if (it->first < 0 || it->first >= static_cast<int>(images_copy.size()) ||
				    it->second < 0 || it->second >= static_cast<int>(images_copy.size())) {
					it = framePairs.erase(it);
				} else {
					++it;
				}
			}
		}

		// Make sure we have valid frame pairs to process
		if (framePairs.empty()) {
			// No valid frame pairs
			result = false;
			m_processing = false;
			m_progress = 1.0f;
			callback(result);
			return result;
		}

		// Process all frame pairs
		for (size_t pairIdx = 0; pairIdx < framePairs.size(); pairIdx++) {
			// Update progress
			m_progress = static_cast<float>(pairIdx) / static_cast<float>(framePairs.size());

			int firstIdx = framePairs[pairIdx].first;
			int secondIdx = framePairs[pairIdx].second;

			// Double-check indices are within bounds
			if (firstIdx < 0 || firstIdx >= static_cast<int>(images_copy.size()) || secondIdx < 0 ||
			    secondIdx >= static_cast<int>(images_copy.size())) {
				// Skip invalid pair
				continue;
			}

			// === INPUT FORMATTING ===
			cv::Mat image1 = cv::Mat(height, width, CV_8UC4, images_copy[firstIdx]);
			cv::cvtColor(image1, image1, cv::COLOR_BGRA2GRAY);
			auto tiles1 = Tiler::CreateTiles(image1, tile_config);
			cv::Mat image2 = cv::Mat(height, width, CV_8UC4, images_copy[secondIdx]);
			cv::cvtColor(image2, image2, cv::COLOR_BGRA2GRAY);
			auto tiles2 = Tiler::CreateTiles(image2, tile_config);

			// Check if both tile sets have the same size
			if (tiles1.size() != tiles2.size() || tiles1.empty()) {
				// Skip if the tile sets are different sizes or empty
				continue;
			}

			std::vector<Tile> outTiles;
			for (size_t k = 0; k < tiles1.size(); ++k) {
				// === MODEL INFERENCE ===
				auto input =
				    torch::cat({to_tensor(tiles1[k].data), to_tensor(tiles2[k].data)}, 1).to(dev);

				auto out = model.forward({input}).toTensor().to(torch::kCPU);

				// pull out planes
				auto t = out.squeeze(0);
				auto r = t.select(0, 0);
				auto g = t.select(0, 1);
				auto z = torch::zeros_like(r);

				// normalize & to uint8
				auto to_u8 = [&](torch::Tensor x) {
					return x.add(2.0).div(4.0).mul(255).clamp(0, 255).to(torch::kUInt8);
				};

				auto ru8 = to_u8(r);
				auto gu8 = to_u8(g);
				auto zu8 = torch::zeros_like(ru8);

				// Merge channels
				int H = ru8.size(0), W = ru8.size(1);
				cv::Mat mr(H, W, CV_8UC1, ru8.data_ptr<uint8_t>());
				cv::Mat mg(H, W, CV_8UC1, gu8.data_ptr<uint8_t>());
				cv::Mat mz(H, W, CV_8UC1, zu8.data_ptr<uint8_t>());

				std::vector<cv::Mat> chans = {mz, mg, mr};

				cv::Mat bgr;
				cv::merge(chans, bgr);
				cv::cvtColor(bgr, bgr, cv::COLOR_BGR2BGRA);

				// Create descriptive position info including source frame indices
				cv::Point pos = tiles1[k].position;

				// Store the resulting tile
				outTiles.push_back({bgr.clone(), pos});

				// Add to output tiles with metadata about source frame
				Tile resultTile = {bgr.clone(), pos, firstIdx}; // Store source frame index
				tiles.push_back(resultTile);
			}

			// Create stitched result
			auto stitched = Tiler::StitchTiles(outTiles, tile_config, image1.size());

			// Store the result in the target frame (in our copy, not the original)
			memcpy(images_copy[firstIdx], stitched.data, width * height * sizeof(uint32_t));
		}

		m_progress = 1.0f;
#else
		result = false;
#endif

		m_processing = false;
		callback(result);
		return result;
	};

	return std::async(std::launch::async, task);
}

// for testing later
/*
bool DeformationAnalysisInterface::TestModelCPPFlow(std::vector<uint32_t *> &images, int width, int height,
						    const int tile_size, const int overlap,
						    std::vector<Tile> &output_tiles) {
#ifdef UI_INCLUDE_TENSORFLOW
	// load once, reuse
	cppflow::model model("assets/models/batch-m4-combo.pb");

	const std::string in_op = "serving_default_input:0";	// verify via saved_model_cli
	const std::string out_op = "StatefulPartitionedCall:0"; // ditto

	for (int i = 0; i + 1 < (int)images.size(); ++i) {
		cv::Mat img1(height, width, CV_8UC4, images[i]);
		cv::cvtColor(img1, img1, cv::COLOR_BGRA2GRAY);
		auto tiles1 = split_tiles(img1, tile_size);

		cv::Mat img2(height, width, CV_8UC4, images[i + 1]);
		cv::cvtColor(img2, img2, cv::COLOR_BGRA2GRAY);
		auto tiles2 = split_tiles(img2, tile_size);

		std::vector<Tile> outTiles;
		for (size_t k = 0; k < tiles1.size(); ++k) {
			// build nhwc float tensor
			std::vector<float> buf(tile_size * tile_size * 2);
			for (int y = 0; y < tile_size; ++y)
				for (int x = 0; x < tile_size; ++x) {
					buf[(y * tile_size + x) * 2 + 0] = tiles1[k].data.at<uchar>(y, x);
					buf[(y * tile_size + x) * 2 + 1] = tiles2[k].data.at<uchar>(y, x);
				}
			cppflow::tensor input(buf, {1, tile_size, tile_size, 2});

			// inference
			auto outputs = model({{in_op, input}}, {out_op});
			auto out_t = outputs[0]; // shape [1,H,W,2]
			auto data = out_t.get_data<float>();

			cv::Mat mr(tile_size, tile_size, CV_8UC1), mg(tile_size, tile_size, CV_8UC1),
			    mz = cv::Mat::zeros(tile_size, tile_size, CV_8UC1);

			// apply norm (x+2)/4*255 clamp
			auto norm_u8 = [](float v) {
				int i = int((v + 2.f) / 4.f * 255.f);
				return uchar(std::min(255, std::max(0, i)));
			};
			for (int y = 0; y < tile_size; ++y)
				for (int x = 0; x < tile_size; ++x) {
					int idx = y * tile_size + x;
					mr.at<uchar>(y, x) = norm_u8(data[idx * 2 + 0]);
					mg.at<uchar>(y, x) = norm_u8(data[idx * 2 + 1]);
				}

			std::vector<cv::Mat> chans = {mz, mg, mr};
			cv::Mat bgr, bgra;
			cv::merge(chans, bgr);
			cv::cvtColor(bgr, bgra, cv::COLOR_BGR2BGRA);

			outTiles.push_back({bgra.clone(), tiles1[k].position});
			output_tiles.push_back({bgra.clone(), tiles1[k].position});
		}
		auto stitched = stitch_tiles(outTiles, img1.size(), tile_size);
		cv::imwrite("cppflow_stitched" + std::to_string(i) + ".png", stitched);
		memcpy(images[i], stitched.data, width * height * sizeof(uint32_t));
	}
	return true;
#else  // UI_INCLUDE_TENSORFLOW
	return false;
#endif // UI_INCLUDE_TENSORFLOW
}
*/
