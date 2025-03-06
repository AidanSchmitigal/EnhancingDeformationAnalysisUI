#include <core/DenoiseInterface.hpp>

#include <cppflow/cppflow.h>

#include <opencv2/opencv.hpp>

#include <iostream>

// Structure to hold tile information
struct ImageTile {
	cv::Mat data;       // The processed tile
	cv::Point position; // Top-left position in original image
	cv::Size size;      // Size of the tile
};

std::vector<ImageTile> splitImageIntoTiles(const cv::Mat& image, int tileSize, int overlap = 0) {
    std::vector<ImageTile> tiles;

    // Calculate padded dimensions
    int step = tileSize - overlap;
    int paddedWidth = ceil((float)image.cols / step) * step + overlap;
    int paddedHeight = ceil((float)image.rows / step) * step + overlap;

    // Pad image with zeros
    cv::Mat paddedImage;
    cv::copyMakeBorder(image, paddedImage, 0, paddedHeight - image.rows, 
                       0, paddedWidth - image.cols, cv::BORDER_CONSTANT, cv::Scalar(1.0f));

    // Split into tiles with overlap
    for (int y = 0; y <= paddedImage.rows - tileSize; y += step) {
        for (int x = 0; x <= paddedImage.cols - tileSize; x += step) {
            cv::Rect roi(x, y, tileSize, tileSize);
            cv::Mat tile = paddedImage(roi).clone();
            tiles.push_back({tile, cv::Point(x, y), cv::Size(tileSize, tileSize)});
        }
    }

    return tiles;
}

cv::Mat reconstructImageFromTiles(const std::vector<ImageTile>& tiles, cv::Size originalSize, int overlap = 0) {
    if (tiles.empty()) {
        std::cerr << "No tiles provided!" << std::endl;
        return cv::Mat();
    }

    // Determine padded size from tiles
    int maxX = 0, maxY = 0;
    for (const auto& tile : tiles) {
        maxX = std::max(maxX, tile.position.x + tile.size.width);
        maxY = std::max(maxY, tile.position.y + tile.size.height);
    }
    cv::Size paddedSize(maxX, maxY);

    // Initialize accumulation and weight matrices
    cv::Mat reconstructed = cv::Mat::zeros(paddedSize, CV_32F); // Float for accumulation
    cv::Mat weight = cv::Mat::zeros(paddedSize, CV_32F);
    int tileSize = tiles[0].size.width;

    // Create linear blend mask for overlap
    cv::Mat blendMask = cv::Mat::ones(tileSize, tileSize, CV_32F);
    if (overlap > 0) {
        for (int y = 0; y < tileSize; ++y) {
            for (int x = 0; x < tileSize; ++x) {
                float wx = (x < overlap) ? (x / (float)overlap) : 
                          (x >= tileSize - overlap ? (tileSize - x - 1) / (float)overlap : 1.0f);
                float wy = (y < overlap) ? (y / (float)overlap) : 
                          (y >= tileSize - overlap ? (tileSize - y - 1) / (float)overlap : 1.0f);
                blendMask.at<float>(y, x) = wx * wy;
            }
        }
    }

    // Blend tiles
    for (const auto& tile : tiles) {
        cv::Rect roi(tile.position, tile.size);

        cv::Mat weightedTile;
        cv::multiply(tile.data, blendMask, weightedTile);
        reconstructed(roi) += weightedTile;
        weight(roi) += blendMask;
    }

    // Normalize and convert back to grayscale
    cv::Mat normalized;
    cv::divide(reconstructed, weight, normalized); // Handle division by zero implicitly

    // Crop to original size
    return normalized(cv::Rect(0, 0, originalSize.width, originalSize.height));
}

// unbelievable amount of allocations in this function...
bool DenoiseInterface::Denoise(std::vector<uint32_t *> &images, int width, int height, const std::string &model_name, const int tile_size, const int overlap) {
	cppflow::model model = cppflow::model("assets/models/" + model_name);

	for (int i = 0; i < images.size(); i++) {
		cv::Mat image = cv::Mat(height, width, CV_8UC4, images[i]);
		cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
		image.convertTo(image, CV_32FC1, 1.0 / 255.0);
		cv::Size paddedSize;
		auto tiles = splitImageIntoTiles(image, tile_size, overlap);

		std::vector<cppflow::tensor> output;
		for (auto& tile : tiles) {
			std::vector<float> image_data;
			for (int y = 0; y < tile.data.rows; y++) {
				for (int x = 0; x < tile.data.cols; x++) {
					image_data.push_back(tile.data.at<float>(y, x));
				}
			}
			cppflow::tensor input = cppflow::tensor(image_data, {1, tile.data.rows, tile.data.cols, 1});

			try {
				auto output2 = model({{ "serving_default_input_gen", input}}, {"StatefulPartitionedCall"});
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
			cv::Mat output_image(tiles[j].size, CV_32FC1);
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

bool DenoiseInterface::Blur(std::vector<uint32_t *> &images, int width, int height, int kernel_size, float sigma) {
	for (int i = 0; i < images.size(); i++) {
		cv::Mat image(height, width, CV_8UC4, images[i]);
		cv::Mat output_image;
		cv::GaussianBlur(image, output_image, cv::Size(kernel_size, kernel_size), sigma);
		output_image.copyTo(image);
		memcpy(images[i], image.data, width * height * 4);
	}
	return true;
}
