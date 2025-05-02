#include "DeformationAnalysisInterface.hpp"

#include <torch/types.h>
#include <utils.h>

#include <opencv2/opencv.hpp>

#ifdef UI_INCLUDE_TENSORFLOW
#include <cppflow/cppflow.h>
#endif

#include <torch/script.h>
#include <torch/torch.h>

// TODO: add option for padding
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
inline cv::Mat stitch_tiles(const std::vector<tile>& tiles, cv::Size original, int tile = 256) {
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

bool DeformationAnalysisInterface::TestModel(std::vector<uint32_t *> &images, int width, int height, int tile_size, int overlap, std::vector<tile>& output_tiles) {
	auto to_tensor = [](const cv::Mat& m) {
		return torch::from_blob(m.data, {1,1,256,256}, torch::kUInt8).to(torch::kFloat32).clone();
	};

	auto dev = torch::cuda::is_available() ? torch::kCUDA : torch::kCPU;
	// input: 2 images, each 256x256 and need to be converted to 1x256x256 (1 channel)
	// input: data format: float between [0, 255]
	// output: 2 images, each 1x256x256 (total output batchx2x256x256)
	// output: data format: float between [-2, 2]
	auto model = torch::jit::load("assets/models/batch-m4-combo.pt");
	model.to(dev);
	model.eval();

	for (int i = 0; i + 1 <= images.size() - 1; ++i) {
		// === INPUT FORMATTING ===
		cv::Mat image = cv::Mat(height, width, CV_8UC4, images[i]);
		cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
		auto tiles = split_tiles(image, tile_size);
		cv::Mat image2 = cv::Mat(height, width, CV_8UC4, images[i + 1]);
		cv::cvtColor(image2, image2, cv::COLOR_BGRA2GRAY);
		auto tiles2 = split_tiles(image2, tile_size);

		std::vector<tile> outTiles;
		for (int k = 0; k < tiles.size(); ++k) {
			// === MODEL INFERENCE ===
			auto input = torch::cat({to_tensor(tiles[k].data),
					to_tensor(tiles2[k].data)}, 1).to(dev);

			auto out = model.forward({input}).toTensor().to(torch::kCPU); // out: torch::Tensor on cpu, shape {1,2,H,W}

			// pull out three {H,W} uint8 planes
			auto t = out.squeeze(0);                // {2,H,W}
			auto r = t.select(0,0);                 // {H,W}
			auto g = t.select(0,1);                 // {H,W}
			auto z = torch::zeros_like(r);          // {H,W}

			// normalize & to uint8 in one go
			auto to_u8 = [&](torch::Tensor x){
				return x
					.add(2.0)      // output is generally within [-2, 2] (it's odd)
					.div(4.0)
					.mul(255)
					.clamp(0,255)
					.to(torch::kUInt8);
			};
			// two channels of actual info, one of zeros to complete a 3-channel image
			auto ru8 = to_u8(r);  // {H,W}
			auto gu8 = to_u8(g);
			auto zu8 = torch::zeros_like(ru8);

			// copy each into a cv::Mat and merge
			int H = ru8.size(0), W = ru8.size(1);
			cv::Mat mr(H, W, CV_8UC1, ru8.data_ptr<uint8_t>());
			cv::Mat mg(H, W, CV_8UC1, gu8.data_ptr<uint8_t>());
			cv::Mat mz(H, W, CV_8UC1, zu8.data_ptr<uint8_t>());

			std::vector<cv::Mat> chans = { mz, mg, mr };  

			// merge the channels into one cv::Mat
			cv::Mat bgr;
			cv::merge(chans, bgr);  // b=0, g=chan1, r=chan0
			// convert to BGRA (add alpha channel)
			cv::cvtColor(bgr, bgr, cv::COLOR_BGR2BGRA);
			outTiles.push_back({bgr.clone(), tiles[k].tl}); // own memory
			output_tiles.push_back({bgr.clone(), tiles[k].tl}); // own memory
		}
		auto stitched = stitch_tiles(outTiles, image.size(), tile_size);
		// write for debugging
		cv::imwrite("stitched" + std::to_string(i) + ".png", stitched);
		// copy back to the original image
		memcpy(images[i], stitched.data, width * height * sizeof(uint32_t));
	}
	return true;
}
