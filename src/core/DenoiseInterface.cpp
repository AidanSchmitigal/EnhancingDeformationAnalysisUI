#include <core/DenoiseInterface.hpp>
#include <core/ThreadPool.hpp>

#include <utils.h>

#include <filesystem>
#include <atomic>

#ifdef UI_INCLUDE_TENSORFLOW
#include <cppflow/cppflow.h>
#endif

#include <opencv2/opencv.hpp>

// Static member initialization
float DenoiseInterface::m_progress = 0.0f;
bool DenoiseInterface::m_is_processing = false;

// Global thread pool for image processing
// This is implemented as a singleton pattern
ThreadPool& GetThreadPool() {
	static ThreadPool thread_pool;
	return thread_pool;
}

// Original synchronous implementation
bool DenoiseInterface::Denoise(std::vector<uint32_t *> &images, int width, int height, const std::string &model_name, const int tile_size, const int overlap) {
	PROFILE_FUNCTION();

#ifdef UI_INCLUDE_TENSORFLOW
	cppflow::model model("assets/models/" + model_name);

	for (int i = 0; i < images.size(); i++) {
		PROFILE_SCOPE(DenoiseOneImage);
		
		m_progress = (float)i / images.size();

		cv::Mat image = cv::Mat(height, width, CV_8UC4, images[i]);
		cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
		image.convertTo(image, CV_32FC1, 1.0 / 255.0);
		cv::Size paddedSize;
		auto tiles = utils::splitImageIntoTiles(image, tile_size, overlap);

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
			tiles[j].data = output_image;
		}
		cv::Mat reconstructed = utils::reconstructImageFromTiles(tiles, image.size(), overlap);
		reconstructed.convertTo(reconstructed, CV_8UC1, 255.0);
		cv::cvtColor(reconstructed, reconstructed, cv::COLOR_GRAY2BGRA);
		memcpy(images[i], reconstructed.data, width * height * 4);
	}
	
	m_progress = 1.0f;
	return true;
#else
	printf("Denoising not available, recompile/use other executable with TensorFlow support\n");
	return false;
#endif
}

// Asynchronous version of Denoise
std::future<bool> DenoiseInterface::DenoiseAsync(std::vector<uint32_t*>& images, int width, int height, 
                                             const std::string& model_name, const int tile_size, 
                                             const int overlap, std::function<void(bool)> callback) {
	// Set processing flag
	m_is_processing = true;
	m_progress = 0.0f;
	
	// Get the thread pool
	auto& pool = GetThreadPool();
	
	// Submit task to thread pool
	auto future = pool.enqueue([&images, width, height, model_name, tile_size, overlap, callback]() {
		bool result = Denoise(images, width, height, model_name, tile_size, overlap);
		
		// When complete, update processing flag and call callback if provided
		m_is_processing = false;
		if (callback) {
			callback(result);
		}
		
		return result;
	});
	
	return future;
}

bool DenoiseInterface::Blur(std::vector<uint32_t *> &images, int width, int height, int kernel_size, float sigma) {
	PROFILE_FUNCTION();
	
	for (int i = 0; i < images.size(); i++) {
		m_progress = (float)i / images.size();
		
		cv::Mat image(height, width, CV_8UC4, images[i]);
		cv::Mat output_image;
		cv::GaussianBlur(image, output_image, cv::Size(kernel_size, kernel_size), sigma);
		output_image.copyTo(image);
		cv::cvtColor(image, image, cv::COLOR_RGBA2BGRA);
		memcpy(images[i], image.data, width * height * 4);
	}
	
	m_progress = 1.0f;
	return true;
}

// Asynchronous version of Blur
std::future<bool> DenoiseInterface::BlurAsync(std::vector<uint32_t*>& images, int width, int height, 
                                          int kernel_size, float sigma, 
                                          std::function<void(bool)> callback) {
	// Set processing flag
	m_is_processing = true;
	m_progress = 0.0f;
	
	// Get the thread pool
	auto& pool = GetThreadPool();
	
	// Submit task to thread pool
	auto future = pool.enqueue([&images, width, height, kernel_size, sigma, callback]() {
		bool result = Blur(images, width, height, kernel_size, sigma);
		
		// When complete, update processing flag and call callback if provided
		m_is_processing = false;
		if (callback) {
			callback(result);
		}
		
		return result;
	});
	
	return future;
}

bool DenoiseInterface::IsProcessing() {
	return m_is_processing;
}

float DenoiseInterface::GetProgress() {
	return m_progress;
}
