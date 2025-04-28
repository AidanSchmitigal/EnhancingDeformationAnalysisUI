#include "DeformationAnalysisInterface.hpp"

#include <utils.h>

#include <opencv2/opencv.hpp>

#ifdef UI_INCLUDE_TENSORFLOW
#include <cppflow/cppflow.h>
#endif

#include <torch/script.h>
#include <torch/torch.h>

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
		int tile_size, int overlap, std::vector<tile>& output_tiles) {

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
			printf("output size: %zu %zu %zu %zu, size %zu\n", out.size(0), out.size(1),
				out.size(2), out.size(3), out.element_size());
			std::vector<float> outFloats;
			std::vector<unsigned char> outData;
			float* dat = out.data_ptr<float>();
			for (int j = 0; j < out.size(2); j++) {
				std::cout << (double)dat[j] << " ";
				outFloats.push_back(std::clamp(((dat[j] + 2) / 4) * 255.0, 0.0, 255.0));
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
			output_tiles.push_back({bgr.clone(), tiles[k].tl}); // own memory
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
