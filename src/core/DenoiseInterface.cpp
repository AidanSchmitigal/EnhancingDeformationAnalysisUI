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
	// Get and print all operation names
	auto ops = model.get_operations();
	std::cout << "Available operations in the model:\n";
	for (const auto& op : ops) {
		std::cout << op << std::endl;
	}

	for (int i = 0; i < images.size(); i++) {
		std::vector<float> image_data;
		image_data.reserve(width * height * 4);
		for (int j = 0; j < width * height * 4; j++) {
			image_data.push_back(images[i][j] / 255.0f);
		}
		cppflow::tensor input = cppflow::tensor(image_data, {1, width, height-1, 1});

		std::cerr << "Starting model...\n";
		std::vector<cppflow::tensor> output;
		try {
			output = model({{ "serving_default_input_gen", input}}, {"StatefulPartitionedCall"});
		}
		catch (const std::runtime_error& e) {
			std::cerr << "Error: " << e.what() << std::endl;
			return false;
		}
		std::cerr << "Model finished\n";

		image_data = output[0].get_data<float>();
		for (int j = 0; j < width * height; j++) {
			images[i][j] = (uint32_t)(image_data[j] * 255.0f);
		}
		std::cerr << "Finished image " << i << "\n";
	}
	return true;
}
