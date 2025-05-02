#include "DeformationAnalysisInterface.hpp"

#include <torch/types.h>
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
		return torch::from_blob(m.data, {1,1,256,256}, torch::kUInt8).to(torch::kFloat32).clone();
	};

	auto dev = torch::cuda::is_available() ? torch::kCUDA : torch::kCPU;
	auto model = torch::jit::load("assets/models/batch-m4-combo.pt");
	model.to(dev);
	model.eval();

	for (int i = 0; i + 1 <= images.size() - 1; ++i) {
		printf("processing image %d and %d of %zu\n", i, i + 1, images.size() - 1);
		cv::Mat image = cv::Mat(height, width, CV_8UC4, images[i]);
		cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
		auto tiles = split_tiles(image, tile_size);
		cv::Mat image2 = cv::Mat(height, width, CV_8UC4, images[i + 1]);
		cv::cvtColor(image2, image2, cv::COLOR_BGRA2GRAY);
		auto tiles2 = split_tiles(image2, tile_size);
		printf("split image %d of (%zu) into %d tiles\n", i, images.size(), (int)tiles.size());
		std::vector<tile> outTiles;
		printf("tiles size %d, tiles2 size %d\n", (int)tiles.size(), (int)tiles2.size());
		for (int k = 0; k < tiles.size(); ++k) {
			auto input = torch::cat({to_tensor(tiles[k].data),
					to_tensor(tiles2[k].data)}, 1).to(dev);
			auto out   = model.forward({input}).toTensor().to(torch::kCPU);         // (1,2,256,256)
			auto t2 = out.squeeze(0);              // {2, H, W}
			// 2. make a zero‐channel
			auto zeros = torch::zeros({1, t2.size(1), t2.size(2)}, t2.options()); // {1, H, W}
			// 3. vstack them (i.e. concat along dim 0)
			auto stacked = torch::cat({t2, zeros}, 0); // {3, H, W}
			// 4. transpose to H×W×3
			auto hwc = stacked.permute({1, 2, 0});     // {H, W, 3}
			// 5. clamp/scale to [0,255] & convert to uint8
			auto hwc_u8 = (hwc.add(2).div(4).mul(255).clamp(0, 255)).to(torch::kUInt8);
			cv::Mat bgr(hwc_u8.size(0), hwc_u8.size(1), CV_8UC3, hwc_u8.data_ptr<uint8_t>());
			cv::cvtColor(bgr, bgr, cv::COLOR_BGR2BGRA);

			outTiles.push_back({bgr.clone(), tiles[k].tl}); // own memory
			output_tiles.push_back({bgr.clone(), tiles[k].tl}); // own memory
		}
		auto stitched = stitch_tiles(outTiles, image.size(), tile_size);
		printf("stitched image %d of (%zu) into %d tiles\n", i, images.size(), (int)outTiles.size());
		cv::imwrite("stitched" + std::to_string(i) + ".png", stitched);
		memcpy(images[i], stitched.data,
				width * height * sizeof(uint32_t));
	}
	return true;
}
