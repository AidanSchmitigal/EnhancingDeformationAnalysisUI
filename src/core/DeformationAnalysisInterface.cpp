#include "DeformationAnalysisInterface.hpp"

#include <utils.h>

#include <opencv2/opencv.hpp>

#ifdef UI_INCLUDE_TENSORFLOW
#include <cppflow/cppflow.h>
#endif

bool DeformationAnalysisInterface::TestModel(std::vector<uint32_t *> &images, int width, int height, const int tile_size, const int overlap) {
#ifdef UI_INCLUDE_TENSORFLOW
	cppflow::model model = cppflow::model("assets/models/batch-m4-combo/");
	for (int i = 0; i < model.get_operations().size(); i++)
		fprintf(stderr, "Operation %d: %s\n", i, model.get_operations()[i].c_str());

	for (int i = 0; i < images.size() - 1; i++) {
		cv::Mat image = cv::Mat(height, width, CV_8UC4, images[i]);
		//cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
		image.convertTo(image, CV_32F, 1.0 / 255.0);
		cv::Mat image2 = cv::Mat(height, width, CV_8UC4, images[i + 1]);
		//cv::cvtColor(image2, image2, cv::COLOR_BGRA2GRAY);
		image2.convertTo(image2, CV_32F, 1.0 / 255.0);
		auto tiles = utils::splitImageIntoTiles(image, tile_size, overlap);
		auto tiles2 = utils::splitImageIntoTiles(image2, tile_size, overlap);

		std::vector<cppflow::tensor> output;
		for (int k = 0; k < tiles.size(); k++) {
			std::vector<float> image_data;
			image_data.reserve(tiles[k].data.rows * tiles[k].data.cols * 2);
			for (int y = 0; y < tiles[k].data.rows; y++) {
				for (int x = 0; x < tiles[k].data.cols; x++) {
					image_data.push_back(tiles[k].data.at<float>(y, x));
				}
			}
			for (int y = 0; y < tiles2[k].data.rows; y++) {
				for (int x = 0; x < tiles2[k].data.cols; x++) {
					image_data.push_back(tiles2[k].data.at<float>(y, x));
				}
			}

			// note the inputs are in a different order than the original python model
			// is that right??
			cppflow::tensor input = cppflow::tensor(image_data, {1, tiles[k].data.rows, tiles[k].data.cols, 2});

			try {
				// originally tried stateful partitioned call but it wanted saver_filename string tensor
				auto output2 = model({{ "serving_default_input.1", input }}, {"PartitionedCall"});
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
		cv::Mat reconstructed = utils::reconstructImageFromTiles(tiles, image.size(), overlap);
		reconstructed.convertTo(reconstructed, CV_8U, 255.0);
		//cv::cvtColor(reconstructed, reconstructed, cv::COLOR_GRAY2BGRA);
		memcpy(images[i], reconstructed.data, width * height * 4);
	}
	return true;
#else
	return false;
#endif
}
