#include "DeformationAnalysisInterface.hpp"

#include <torch/types.h>
#include <utils.h>

#include <opencv2/opencv.hpp>

#ifdef UI_INCLUDE_TENSORFLOW
#include <cppflow/cppflow.h>
#endif

#include <torch/script.h>
#include <torch/torch.h>

bool DeformationAnalysisInterface::m_processing = false;

// ---- split bgra mat into possibly overlapping tiles ----
inline std::vector<Tile> split_tiles(const cv::Mat& img,
                                     int tileSize = 256,
                                     int overlap  = 0)
{
    if (img.empty()) throw std::runtime_error("split_tiles: empty input");
    int step = tileSize - overlap;
    if (step <= 0) throw std::runtime_error("split_tiles: overlap must be < tileSize");
    std::vector<Tile> out;
    for (int y = 0; y < img.rows; y += step) {
        for (int x = 0; x < img.cols; x += step) {
            int w = std::min(tileSize, img.cols - x);
            int h = std::min(tileSize, img.rows - y);
            cv::Mat roi = img(cv::Rect(x, y, w, h)).clone();
            out.push_back({roi, {x, y}});
            if (x + w >= img.cols) break;
        }
        if (y + std::min(tileSize, img.rows - y) >= img.rows) break;
    }
    return out;
}

// ---- reconstruct full bgra mat from tiles ----
inline cv::Mat stitch_tiles(const std::vector<Tile>& tiles,
                            cv::Size original,
                            int tileSize = 256,
                            int overlap  = 0)
{
    if (tiles.empty()) throw std::runtime_error("stitch_tiles: no tiles");
    cv::Mat canvas(original, CV_8UC4, cv::Scalar(0,0,0,0));
    for (auto& t : tiles) {
        int w = t.data.cols;
        int h = t.data.rows;
        cv::Rect dst(t.position.x, t.position.y, w, h);
        // ensure dst inside canvas
        dst.width  = std::min(dst.width,  original.width  - dst.x);
        dst.height = std::min(dst.height, original.height - dst.y);
        if (dst.width <= 0 || dst.height <= 0) continue;
        cv::Mat sub = t.data(cv::Rect(0, 0, dst.width, dst.height));
        sub.copyTo(canvas(dst));
    }
    return canvas;
}

bool DeformationAnalysisInterface::RunModel(std::vector<uint32_t *> &images, int width, int height, int tile_size, int overlap, std::vector<Tile>& output_tiles) {
	PROFILE_FUNCTION();

#ifdef UI_INCLUDE_PYTORCH
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
		auto tiles = split_tiles(image, tile_size, overlap);
		cv::Mat image2 = cv::Mat(height, width, CV_8UC4, images[i + 1]);
		cv::cvtColor(image2, image2, cv::COLOR_BGRA2GRAY);
		auto tiles2 = split_tiles(image2, tile_size, overlap);

		std::vector<Tile> outTiles;
		for (int k = 0; k < tiles.size(); ++k) {
			// === MODEL INFERENCE ===
			auto input = torch::cat({to_tensor(tiles[k].data),
					to_tensor(tiles2[k].data)}, 1).to(dev);

			auto out = model.forward({input}).toTensor().to(torch::kCPU); // out: torch::Tensor on cpu, shape 1,2,H,W

			// pull out three {H,W} uint8 planes
			auto t = out.squeeze(0);
			auto r = t.select(0,0);
			auto g = t.select(0,1);
			auto z = torch::zeros_like(r);

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
			auto ru8 = to_u8(r);  // H,W
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
			outTiles.push_back({bgr.clone(), tiles[k].position}); // own memory
			output_tiles.push_back({bgr.clone(), tiles[k].position}); // own memory
		}
		auto stitched = stitch_tiles(outTiles, image.size(), tile_size);

		// copy back to the original image
		memcpy(images[i], stitched.data, width * height * sizeof(uint32_t));
	}
	return true;
#else // UI_INCLUDE_PYTORCH
	return false;
#endif // UI_INCLUDE_PYTORCH
}

// for testing later
bool DeformationAnalysisInterface::TestModelCPPFlow(
		std::vector<uint32_t*>& images,
		int width,
		int height,
		const int tile_size,
		const int overlap,
		std::vector<Tile>& output_tiles)
{
#ifdef UI_INCLUDE_TENSORFLOW
	// load once, reuse
	cppflow::model model("assets/models/batch-m4-combo.pb");

	const std::string in_op  = "serving_default_input:0";      // verify via saved_model_cli
	const std::string out_op = "StatefulPartitionedCall:0";    // ditto

	for (int i = 0; i + 1 < (int)images.size(); ++i) {
		cv::Mat img1(height, width, CV_8UC4, images[i]);
		cv::cvtColor(img1, img1, cv::COLOR_BGRA2GRAY);
		auto tiles1 = split_tiles(img1, tile_size);

		cv::Mat img2(height, width, CV_8UC4, images[i+1]);
		cv::cvtColor(img2, img2, cv::COLOR_BGRA2GRAY);
		auto tiles2 = split_tiles(img2, tile_size);

		std::vector<Tile> outTiles;
		for (size_t k = 0; k < tiles1.size(); ++k) {
			// build nhwc float tensor
			std::vector<float> buf(tile_size * tile_size * 2);
			for (int y = 0; y < tile_size; ++y)
				for (int x = 0; x < tile_size; ++x) {
					buf[(y*tile_size + x)*2 + 0] = tiles1[k].data.at<uchar>(y,x);
					buf[(y*tile_size + x)*2 + 1] = tiles2[k].data.at<uchar>(y,x);
				}
			cppflow::tensor input(
					buf, {1, tile_size, tile_size, 2});

			// inference
			auto outputs = model({{in_op, input}}, {out_op});
			auto out_t = outputs[0]; // shape [1,H,W,2]
			auto data = out_t.get_data<float>();

			cv::Mat mr(tile_size, tile_size, CV_8UC1),
				mg(tile_size, tile_size, CV_8UC1),
				mz = cv::Mat::zeros(tile_size, tile_size, CV_8UC1);

			// apply norm (x+2)/4*255 clamp
			auto norm_u8 = [](float v){
				int i = int((v+2.f)/4.f*255.f);
				return uchar(std::min(255, std::max(0, i)));
			};
			for (int y=0; y<tile_size; ++y) for (int x=0; x<tile_size; ++x){
				int idx = y*tile_size + x;
				mr.at<uchar>(y,x) = norm_u8(data[idx*2 + 0]);
				mg.at<uchar>(y,x) = norm_u8(data[idx*2 + 1]);
			}

			std::vector<cv::Mat> chans = { mz, mg, mr };
			cv::Mat bgr, bgra;
			cv::merge(chans, bgr);
			cv::cvtColor(bgr, bgra, cv::COLOR_BGR2BGRA);

			outTiles.push_back({bgra.clone(), tiles1[k].position});
			output_tiles.push_back({bgra.clone(), tiles1[k].position});
		}
		auto stitched = stitch_tiles(outTiles, img1.size(), tile_size);
		cv::imwrite("cppflow_stitched"+std::to_string(i)+".png", stitched);
		memcpy(images[i], stitched.data, width*height*sizeof(uint32_t));
	}
	return true;
#else // UI_INCLUDE_TENSORFLOW
	return false;
#endif // UI_INCLUDE_TENSORFLOW
}
