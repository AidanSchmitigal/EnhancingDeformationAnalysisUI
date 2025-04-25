#include "DeformationAnalysisInterface.hpp"

#include <utils.h>

#include <opencv2/opencv.hpp>

#ifdef UI_INCLUDE_TENSORFLOW
#include <cppflow/cppflow.h>
#endif

#include <torch/script.h>

/*
   bool DeformationAnalysisInterface::TestModel(std::vector<uint32_t *> &images,
   int width, int height, const int tile_size, const int overlap) { #ifdef
   UI_INCLUDE_TENSORFLOW cppflow::model model =
   cppflow::model("assets/models/batch-m4-combo/"); for (int i = 0; i <
   model.get_operations().size(); i++) fprintf(stderr, "Operation %d: %s\n", i,
   model.get_operations()[i].c_str());

   for (int i = 0; i < images.size() - 1; i++) {
   cv::Mat image = cv::Mat(height, width, CV_8UC4, images[i]);
   cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
   image.convertTo(image, CV_32F, 1.0 / 255.0);
   cv::Mat image2 = cv::Mat(height, width, CV_8UC4, images[i + 1]);
   cv::cvtColor(image2, image2, cv::COLOR_BGRA2GRAY);
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
cppflow::tensor input = cppflow::tensor(image_data, {1, tiles[k].data.rows,
tiles[k].data.cols, 2});

try {
// originally tried stateful partitioned call but it wanted saver_filename
string tensor auto output2 = model({{ "serving_default_input", input }},
{"PartitionedCall"}); output.push_back(output2[0]);
}
catch (const std::runtime_error& e) {
std::cerr << "Error: " << e.what() << std::endl;
return false;
}
}

// convert tensors to cv::Mat and recombine
for (int j = 0; j < tiles.size(); j++) {
auto output_data = output[j].get_data<float>();
printf("Output size: %zu\n", output_data.size());
printf("Tile size: %d x %d = %d\n", tiles[j].size.width, tiles[j].size.height,
tiles[j].size.width * tiles[j].size.height); cv::Mat output_image(tiles[j].size,
CV_32F); for (int y = 0; y < tiles[j].size.height; y++) { for (int x = 0; x <
tiles[j].size.width; x++) { output_image.at<float>(y, x) = output_data[y *
tiles[j].size.width + x];
}
}
tiles[j].data = output_image;
}
cv::Mat reconstructed = utils::reconstructImageFromTiles(tiles, image.size(),
overlap); cv::cvtColor(reconstructed, reconstructed, cv::COLOR_GRAY2BGRA);
reconstructed.convertTo(reconstructed, CV_8UC4, 255.0);
memcpy(images[i], reconstructed.data, width * height * 4);
}
return true;
#else
return false;
#endif
}
*/

#include <filesystem>
#include <opencv2/opencv.hpp>
#include <torch/script.h>
#include <torch/torch.h>
#include <vector>

struct tile {
	cv::Mat data; // tile data
	cv::Point tl; // top-left corner of tile in original image
};

// ---- split any grayscale image into zero-padded 256×256 tiles ----
inline std::vector<tile> split_tiles(const cv::Mat& img, int tileSize = 256) {
	if (img.empty()) throw std::runtime_error("no image to split");

	std::vector<tile> out;
	for (int y = 0; y < img.rows; y += tileSize)
		for (int x = 0; x < img.cols; x += tileSize) {
			int h = std::min(tileSize, img.rows - y);
			int w = std::min(tileSize, img.cols - x);
			cv::Mat roi = img(cv::Rect(x, y, w, h));

			cv::Mat pad(tileSize, tileSize, img.type(), cv::Scalar::all(0));
			roi.copyTo(pad(cv::Rect(0,0,w,h)));

			out.push_back({pad, {x,y}});
		}
	return out;
}

// ---- glue colour tiles back into full image ----
inline cv::Mat stitch_tiles(const std::vector<tile>& tiles,
		cv::Size original, int tile = 256)
{
	if (tiles.empty()) throw std::runtime_error("no tiles to stitch");

	cv::Mat canvas(original, tiles[0].data.type(), cv::Scalar::all(0));
	for (const auto& t : tiles) {
		int w = std::min(tile, original.width  - t.tl.x);
		int h = std::min(tile, original.height - t.tl.y);
		t.data(cv::Rect(0,0,w,h))
			.copyTo(canvas(cv::Rect(t.tl.x, t.tl.y, w, h)));
	}
	return canvas;
}

bool DeformationAnalysisInterface::TestModel(
		std::vector<uint32_t *> &images,
		int width, int height,
		int tile_size, int overlap) {

	auto to_tensor = [](const cv::Mat& m) {
		return torch::from_blob(m.data, {1,1,256,256}).clone();
	};

	auto dev = torch::cuda::is_available() ? torch::kCUDA : torch::kCPU;
	auto model = torch::jit::load("assets/models/batch-m4-combo.pt");
	model.to(dev);
	model.eval();

	for (int i = 0; i + 1 < images.size(); ++i) {
		cv::Mat image = cv::Mat(height, width, CV_8UC4, images[i]);
		cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
		auto tiles = split_tiles(image, tile_size);
		cv::Mat image2 = cv::Mat(height, width, CV_8UC4, images[i + 1]);
		cv::cvtColor(image2, image2, cv::COLOR_BGRA2GRAY);
		auto tiles2 = split_tiles(image2, tile_size);
		std::vector<tile> outTiles;
		for (int k = 0; k < tiles.size(); ++k) {
			auto input = torch::cat({to_tensor(tiles[k].data),
				to_tensor(tiles2[k].data)}, 1).to(dev);
			auto out   = model.forward({input}).toTensor().to(torch::kCPU);         // (1,2,256,256)
			std::vector<float> outFloats;
			std::vector<unsigned char> outData;
			float* dat = out[0].data_ptr<float>();
			for (int j = 0; j < out.size(0); j++) {
				outFloats.push_back(std::clamp(((dat[j] + 2) / 4) * 255.0f, 0.0f, 255.0f));
			}
			for (int j = 0; j < outFloats.size(); j++) {
				outData.push_back((unsigned char)dat[j]);
			}
			
			std::vector<cv::Mat> chans(3);
			for (int j = 0; j < 2; ++j) {
				chans[j] = cv::Mat(256, 256, CV_8U, outData.data() + j * 256 * 256);
			}
			chans[2] = cv::Mat(256,256,CV_8U);
			cv::Mat rgb; cv::merge(chans, rgb);
			cv::Mat bgr; cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
			outTiles.push_back({bgr.clone(), tiles[k].tl}); // own memory
		}
		auto stitched = stitch_tiles(outTiles, image.size(), tile_size);
		cv::cvtColor(stitched, stitched, cv::COLOR_BGR2BGRA);
		stitched.convertTo(stitched, CV_8UC4, 255.0);
		cv::imwrite("stitched" + std::to_string(i) + ".png", stitched);
		memcpy(images[i], stitched.data,
				width * height * sizeof(uint32_t));
	}
	return true;
}
