#include "DeformationAnalysisInterface.hpp"

#include <opencv2/opencv.hpp>

#include <cppflow/cppflow.h>
#include <tensorflow/c/c_api.h>

// Function to split an image into tiles
std::vector<DeformationAnalysisInterface::ImageTile> DeformationAnalysisInterface::splitImageIntoTiles(const cv::Mat& image, int tileSize, int overlap) {
	std::vector<ImageTile> tiles;

	int step = tileSize - overlap;  // Move by tileSize - overlap
	for (int y = 0; y <= image.rows - tileSize; y += step) {
		if (y + tileSize > image.rows) y = image.rows - tileSize;
		for (int x = 0; x <= image.cols - tileSize; x += step) {
			if (x + tileSize > image.cols) x = image.cols - tileSize;
			// Extract tile
			cv::Rect roi(x, y, tileSize, tileSize);
			cv::Mat tile = image(roi).clone();  // Copy to avoid reference issues

			// Store tile info
			tiles.push_back({tile, cv::Point(x, y), cv::Size(tileSize, tileSize)});
		}
	}

	return tiles;
}

cv::Mat DeformationAnalysisInterface::reconstructImageFromTiles(const std::vector<ImageTile>& tiles, cv::Size originalSize, int overlap) {
	if (tiles.empty()) {
		std::cerr << "No tiles provided!" << std::endl;
		return cv::Mat();
	}

	cv::Mat reconstructed = cv::Mat::zeros(originalSize, tiles[0].data.type()); // Output image
	cv::Mat weight = cv::Mat::zeros(originalSize, CV_32F); // Accumulate blending weights

	int tileSize = tiles[0].size.width; // Assuming square tiles

	// Create a blending mask to smooth tile overlaps (cosine ramp)
	cv::Mat blendMask = cv::Mat::ones(tileSize, tileSize, CV_32F);
	for (int y = 0; y < tileSize; ++y) {
		for (int x = 0; x < tileSize; ++x) {
			float wx = (x < overlap) ? (x / float(overlap)) : (x > tileSize - overlap ? (tileSize - x) / float(overlap) : 1.0f);
			float wy = (y < overlap) ? (y / float(overlap)) : (y > tileSize - overlap ? (tileSize - y) / float(overlap) : 1.0f);
			blendMask.at<float>(y, x) = wx * wy; // Combine horizontal and vertical weights
		}
	}

	// Iterate over each tile and blend it into the reconstructed image
	for (const auto& tile : tiles) {
		cv::Rect roi(tile.position, tile.size);

		// Convert tile data to float for blending
		cv::Mat tileFloat;
		tile.data.copyTo(tileFloat);

		// Apply blending mask
		cv::Mat weightedTile;
		cv::multiply(tileFloat, blendMask, weightedTile);

		// Accumulate weighted tile values and blending weights
		reconstructed(roi) += weightedTile;
		weight(roi) += blendMask;
	}

	// Normalize by accumulated weights (avoid division by zero)
	cv::Mat normalized;
	reconstructed.copyTo(normalized);
	for (int y = 0; y < reconstructed.rows; y++) {
		for (int x = 0; x < reconstructed.cols; x++) {
			if (weight.at<float>(y, x) > 0.0f) {
				normalized.at<float>(y, x) = reconstructed.at<float>(y, x) / weight.at<float>(y, x);
			}
		}
	}

	return normalized;
}

// unbelievable amount of allocations in this function...
bool DeformationAnalysisInterface::TestModel(std::vector<uint32_t *> &images, int width, int height, const int tile_size, const int overlap) {
	cppflow::model model = cppflow::model("assets/test_model");
	auto ops = model.get_operations();
	for (auto& op : ops) {
		std::cerr << op << std::endl;
	}

	for (int i = 0; i < images.size(); i++) {
		cv::Mat image = cv::Mat(height, width, CV_8UC4, images[i]);
		cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
		image.convertTo(image, CV_32FC1, 1.0 / 255.0);
		auto tiles = splitImageIntoTiles(image, tile_size, overlap);
		std::cerr << "Tiles: " << tiles.size() << std::endl;

		std::vector<cppflow::tensor> output;
		for (auto& tile : tiles) {
			std::vector<float> image_data;
			for (int y = 0; y < tile.data.rows; y++) {
				for (int x = 0; x < tile.data.cols; x++) {
					image_data.push_back(tile.data.at<float>(y, x));
				}
			}
			for (int y = 0; y < tile.data.rows; y++) {
				for (int x = 0; x < tile.data.cols; x++) {
					image_data.push_back(tile.data.at<float>(y, x));
				}
			}
			cppflow::tensor input = cppflow::tensor(image_data, {1, 2, tile.data.rows, tile.data.cols});

			try {
				std::string str = "saver";
				std::vector<int64_t> dims = {1}; // Shape {1} for one string
				size_t data_size = sizeof(int64_t) * 2 + str.size(); // 2 offsets + string bytes
				std::vector<char> buffer(data_size);

				// Offset for one string: [0, length]
				int64_t offset_start = 0;
				int64_t offset_end = str.size();
				memcpy(buffer.data(), &offset_start, sizeof(int64_t));
				memcpy(buffer.data() + sizeof(int64_t), &offset_end, sizeof(int64_t));
				memcpy(buffer.data() + 2 * sizeof(int64_t), str.data(), str.size());

				// Create the tensor
				TF_Tensor* tensor = TF_NewTensor(
						TF_STRING,           // Data type
						dims.data(),         // Dimensions
						dims.size(),         // Number of dimensions (1)
						buffer.data(),       // Data buffer
						data_size,           // Buffer size
						[](void* data, size_t, void*) { }, // Deallocator
						nullptr              // Deallocator arg
						);
				cppflow::tensor filename_data = cppflow::tensor(tensor);
				auto output2 = model({
						{ "serving_default_input.1", input },
						{ "saver_filename",  filename_data }
						},
						{"StatefulPartitionedCall"});
				printf("Model finished!\n");
				output.push_back(output2[0]);
			}
			catch (const std::runtime_error& e) {
				std::cerr << "Error: " << e.what() << std::endl;
				return false;
			}
		}

		// convert tensors to cv::Mat and recombine
		for (int j = 0; j < tiles.size(); j++) {
			auto output_data = output[j].get_data<float>();
			cv::Mat output_image(tiles[j].size, CV_32F);
			for (int y = 0; y < tiles[j].size.height; y++) {
				for (int x = 0; x < tiles[j].size.width; x++) {
					output_image.at<float>(y, x) = output_data[y * tiles[j].size.width + x];
				}
			}
			//output_image.convertTo(output_image, CV_8UC1, 255.0);
			tiles[j].data = output_image;
		}
		cv::Mat reconstructed = reconstructImageFromTiles(tiles, image.size(), overlap);
		reconstructed.convertTo(reconstructed, CV_8UC1, 255.0);
		cv::cvtColor(reconstructed, reconstructed, cv::COLOR_GRAY2BGRA);
		memcpy(images[i], reconstructed.data, width * height * 4);
	}
	return true;
}
