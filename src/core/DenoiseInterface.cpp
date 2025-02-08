#include <core/DenoiseInterface.hpp>

#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <cppflow/cppflow.h>

#include <opencv2/opencv.hpp>

#include <iostream>

namespace py = pybind11;

cv::Mat numpy_to_mat(py::array_t<uint8_t> input_array) {
	py::buffer_info buf = input_array.request();
	int height = buf.shape[0];
	int width = buf.shape[1];

	cv::Mat mat(height, width, CV_8U, buf.ptr);
	return mat.clone();  // Clone to prevent Python from managing memory
}

py::array_t<uint8_t> mat_to_numpy(const cv::Mat& mat) {
	auto shape = std::vector<std::size_t>{static_cast<size_t>(mat.rows), static_cast<size_t>(mat.cols)};
	return py::array_t<uint8_t>(shape, mat.ptr<uint8_t>());
}

bool DenoiseInterface::Denoise(std::vector<uint32_t*>& images, int width, int height, const std::string& model_name, int kernel_size, float sigma) {
	if (model_name == "Blur") {
		for (int i = 0; i < images.size(); i++) {
			cv::Mat image(height, width, CV_8UC4, images[i]);
			cv::Mat output_image;
			cv::GaussianBlur(image, output_image, cv::Size(kernel_size, kernel_size), sigma);
			output_image.copyTo(image);
			memcpy(images[i], image.data, width * height * 4);
		}
		return true;
	}

	py::scoped_interpreter guard{};

	try {
		py::module_ sys = py::module_::import("sys");
		sys.attr("path").attr("append")("/home/adam/School/CS46x/EnhancingDeformationAnalysisUI/src/python");
		// Load the Python module dynamically
		py::module_ denoise_module = py::module_::import("denoise_processor");

		int tileWidth = 256;
		int tileHeight = 256;
		std::vector<std::vector<uint8_t*>> image_sequence;
		for (int i = 0; i < images.size(); i++) {

			cv::Mat image(height, width, CV_8UC4, images[i]);

			// Check if the image can be evenly divided
			int rows = image.rows / tileHeight;
			int cols = image.cols / tileWidth;

			std::vector<uint8_t*> tiles;
			for (int y = 0; y < rows; ++y) {
				for (int x = 0; x < cols; ++x) {
					// Define ROI (Region of Interest)
					uint8_t* data = (uint8_t*)malloc(tileWidth * tileHeight * 4);
					cv::Rect roi(x * tileWidth, y * tileHeight, tileWidth, tileHeight);
					memcpy(data, image(roi).data, tileWidth * tileHeight * 4);
					tiles.push_back(data);  // Clone ensures independent copies
				}
			}
			image_sequence.push_back(tiles);
		}
		py::list image_sequence_py = py::cast(image_sequence);

		py::object denoise_class = denoise_module.attr("denoise_processor");
		std::cout << "Calling function..." << std::endl;
		// Call the denoise function in Python
		py::object result = denoise_class.attr("denoise")(denoise_class, image_sequence, model_name);
		std::cout << "Function called" << std::endl;

		for (int i = 0; i < images.size(); i++) {
			// Convert back from Python list to OpenCV Mat
			cv::Mat output_image(width, height, CV_8U);
			for (int i = 0; i < 4; ++i) {
				for (int j = 0; j < 4; ++j) {
					auto tile_obj = result.cast<py::list>()[i].cast<py::list>()[j].cast<py::tuple>();
					cv::Mat tile_data = numpy_to_mat(tile_obj[0].cast<py::array_t<uint8_t>>());
					cv::Rect roi(j * tileWidth, i * tileHeight, tileWidth, tileHeight);
					tile_data.copyTo(output_image(roi));
				}
			}

			memcpy(images[i], output_image.data, width * height * 4);
		}
		for (auto& image : image_sequence) {
			for (auto& tile : image) {
				free(tile);
			}
		}
	} catch (const py::error_already_set& e) {
		std::cerr << "Python error: " << e.what() << std::endl;
		return false;
	}

	return true;
}

// Structure to hold tile information
struct ImageTile {
	cv::Mat data;       // The processed tile
	cv::Point position; // Top-left position in original image
	cv::Size size;      // Size of the tile
};

// Function to split an image into tiles
std::vector<ImageTile> splitImageIntoTiles(const cv::Mat& image, int tileSize, int overlap = 0) {
	std::vector<ImageTile> tiles;

	int step = tileSize - overlap;  // Move by tileSize - overlap
	for (int y = 0; y <= image.rows - tileSize; y += step) {
		for (int x = 0; x <= image.cols - tileSize; x += step) {
			// Extract tile
			cv::Rect roi(x, y, tileSize, tileSize);
			cv::Mat tile = image(roi).clone();  // Copy to avoid reference issues

			// Store tile info
			tiles.push_back({tile, cv::Point(x, y), cv::Size(tileSize, tileSize)});
		}
	}

	return tiles;
}

// Function to reconstruct the image from tiles
cv::Mat reconstructImageFromTiles(const std::vector<ImageTile>& tiles, cv::Size originalSize, int overlap) {
	if (tiles.empty()) {
		std::cerr << "No tiles provided!" << std::endl;
		return cv::Mat();
	}

	cv::Mat reconstructed = cv::Mat::zeros(originalSize, tiles[0].data.type()); // Empty image
	cv::Mat weight = cv::Mat::zeros(originalSize, CV_32F);  // Accumulate blending weights

	int tileSize = tiles[0].size.width;  // Assuming square tiles

	// Iterate over each tile and place it in the reconstructed image
	for (const auto& tile : tiles) {
		cv::Rect roi(tile.position, tile.size);

		// Add tile values to reconstructed image
		reconstructed(roi) += tile.data;

		// Create a weight mask (for averaging overlapping regions)
		cv::Mat tileWeight = cv::Mat::ones(tile.size, CV_32F);
		weight(roi) += tileWeight;
	}

	// Normalize by dividing by the accumulated weight (to handle overlaps)
	for (int y = 0; y < reconstructed.rows; y++) {
		for (int x = 0; x < reconstructed.cols; x++) {
			if (weight.at<float>(y, x) > 0) {  
				reconstructed.at<uchar>(y, x) = static_cast<uchar>(
						reconstructed.at<uchar>(y, x) / weight.at<float>(y, x));
			}
		}
	}

	return reconstructed;
}

// unbelievable amount of allocations in this function...
bool DenoiseInterface::DenoiseNew(std::vector<uint32_t *> &images, int width, int height, const std::string &model_name, int kernel_size, float sigma) {
	if (model_name == "Blur") {
		for (int i = 0; i < images.size(); i++) {
			cv::Mat image(height, width, CV_8UC4, images[i]);
			cv::Mat output_image;
			cv::GaussianBlur(image, output_image, cv::Size(kernel_size, kernel_size), sigma);
			output_image.copyTo(image);
			memcpy(images[i], image.data, width * height * 4);
		}
		return true;
	}

	cppflow::model model = cppflow::model("../assets/models/" + model_name);

	for (int i = 0; i < images.size(); i++) {
		cv::Mat image = cv::Mat(height, width, CV_8UC4, images[i]);
		cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
		image.convertTo(image, CV_32FC1, 1.0 / 255.0);
		auto tiles = splitImageIntoTiles(image, 256, 0);

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
			cv::Mat output_image(tiles[j].size, CV_32F);
			for (int y = 0; y < tiles[j].size.height; y++) {
			for (int x = 0; x < tiles[j].size.width; x++) {
				output_image.at<float>(y, x) = output_data[y * tiles[j].size.width + x];
			}
			}
			output_image.convertTo(output_image, CV_8U, 255.0);
			tiles[j].data = output_image;
		}
		cv::Mat reconstructed = reconstructImageFromTiles(tiles, image.size(), 0);
		cv::cvtColor(reconstructed, reconstructed, cv::COLOR_GRAY2BGRA);
		memcpy(images[i], reconstructed.data, width * height * 4);
	}
	return true;
}
