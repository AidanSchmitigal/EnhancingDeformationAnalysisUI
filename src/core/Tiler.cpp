#include <core/Tiler.hpp>

std::vector<Tile> Tiler::CreateTiles(const cv::Mat &image, const TileConfig &config) {
	if (config.type == TileType::Cropped) {
		return CreateCroppedTiles(image, config);
	} else if (config.type == TileType::Blended) {
		return CreateBlendedTiles(image, config);
	}
	return {};
}

cv::Mat Tiler::StitchTiles(const std::vector<Tile> &tiles, const TileConfig &config, const cv::Size &originalSize,
			   const bool singleChannel) {
	if (config.type == TileType::Cropped) {
		return StitchCroppedTiles(tiles, originalSize, config, singleChannel);
	} else if (config.type == TileType::Blended) {
		return StitchBlendedTiles(tiles, originalSize, config, singleChannel);
	}
	return {};
}

std::vector<Tile> Tiler::CreateCroppedTiles(const cv::Mat &image, const TileConfig &config) {
	auto tileSize = config.tileSize;
	auto centerSize = config.centerSize;
	auto includeOutside = config.includeOutside;

	int height = image.rows;
	int width = image.cols;
	int centerInset = (tileSize - centerSize) / 2;
	std::vector<Tile> tiles;

	int y0 = includeOutside ? -centerInset : 0;
	for (int y = y0; y < height; y += centerSize) {
		for (int x = y0; x < width; x += centerSize) {
			int yStart = std::max(y, 0);
			int xStart = std::max(x, 0);
			int yEnd = std::min(y + tileSize, height);
			int xEnd = std::min(x + tileSize, width);

			int shiftY = yStart - y;
			int shiftX = xStart - x;

			cv::Mat roi = image(cv::Range(yStart, yEnd), cv::Range(xStart, xEnd));
			cv::Mat tile;
			if (roi.rows != tileSize || roi.cols != tileSize) {
				tile = cv::Mat::zeros(tileSize, tileSize, image.type());
				roi.copyTo(tile(cv::Rect(shiftX, shiftY, roi.cols, roi.rows)));
			} else {
				tile = roi.clone();
			}

			tiles.push_back({tile, cv::Point(x, y)});
		}
	}
	return tiles;
}

std::vector<Tile> Tiler::CreateBlendedTiles(const cv::Mat &image, const TileConfig &config) {
	auto tileSize = config.tileSize;
	auto overlap = config.overlap;

	std::vector<Tile> tiles;
	int h = image.rows, w = image.cols;
	for (int y = 0; y < h; y += tileSize - overlap) {
		for (int x = 0; x < w; x += tileSize - overlap) {
			int yEnd = std::min(y + tileSize, h);
			int xEnd = std::min(x + tileSize, w);
			cv::Rect roi(x, y, xEnd - x, yEnd - y);
			cv::Mat tile = image(roi);
			if (tile.rows != tileSize || tile.cols != tileSize) {
				cv::Mat padded = cv::Mat::zeros(tileSize, tileSize, image.type());
				tile.copyTo(padded(cv::Rect(0, 0, tile.cols, tile.rows)));
				tile = padded;
			}
			tiles.push_back({tile, cv::Point(x, y)});
		}
	}
	return tiles;
}

cv::Mat Tiler::StitchCroppedTiles(const std::vector<Tile> &tiles, const cv::Size &originalSize,
				  const TileConfig &config, const bool singleChannel) {
	int tileSize = config.tileSize;
	int centerSize = config.centerSize;
	bool includeOutside = config.includeOutside;
	int inset = (tileSize - centerSize) / 2;
	int h = originalSize.height;
	int w = originalSize.width;

	int cvType = singleChannel ? CV_32F : CV_32FC2;
	cv::Mat result(h, w, cvType, cv::Scalar::all(0));

	for (auto &tile : tiles) {
		int x = tile.position.x;
		int y = tile.position.y;

		// compute dest bounds
		int cx0 = std::max(x + inset, 0);
		int cy0 = std::max(y + inset, 0);
		int cx1 = std::min(x + tileSize - inset, w);
		int cy1 = std::min(y + tileSize - inset, h);

		// default src bounds (center region)
		int tx0 = inset;
		int ty0 = inset;
		int tx1 = cx1 - x;
		int ty1 = cy1 - y;

		if (!includeOutside) {
			if (y < centerSize) {
				cy0 = 0;
				ty0 = 0;
			}
			if (y + tileSize > h) {
				cy1 = h;
				ty1 = std::min(tileSize, h - y);
			}
			if (x < centerSize) {
				cx0 = 0;
				tx0 = 0;
			}
			if (x + tileSize > w) {
				cx1 = w;
				tx1 = std::min(tileSize, w - x);
			}
		}

		if (singleChannel) {
			int copyW = cx1 - cx0;
			int copyH = cy1 - cy0;
			if (copyW <= 0 || copyH <= 0)
				continue;

			// recompute src offsets for 1ch
			tx0 = inset + (cx0 - (x + inset));
			ty0 = inset + (cy0 - (y + inset));
			tx0 = std::clamp(tx0, 0, tileSize - 1);
			ty0 = std::clamp(ty0, 0, tileSize - 1);

			if (tx0 + copyW > tileSize)
				copyW = tileSize - tx0;
			if (ty0 + copyH > tileSize)
				copyH = tileSize - ty0;
			if (copyW <= 0 || copyH <= 0)
				continue;

			cv::Mat srcROI = tile.data(cv::Rect(tx0, ty0, copyW, copyH));
			cv::Mat dstROI = result(cv::Rect(cx0, cy0, copyW, copyH));
			srcROI.copyTo(dstROI);
		} else {
			// two-channel float case
			cv::Mat srcROI = tile.data(cv::Range(ty0, ty1), cv::Range(tx0, tx1));
			cv::Mat dstROI = result(cv::Range(cy0, cy1), cv::Range(cx0, cx1));
			srcROI.copyTo(dstROI);
		}
	}

	return result;
}

cv::Mat Tiler::StitchBlendedTiles(const std::vector<Tile> &tiles, const cv::Size &originalSize,
				  const TileConfig &config, const bool singleChannel) {
	int tileSize = config.tileSize;
	int overlap = config.overlap;
	int w = originalSize.width;
	int h = originalSize.height;
	int ch = singleChannel ? 1 : (tiles.empty() ? 1 : tiles[0].data.channels());

	cv::Mat result(h, w, CV_MAKETYPE(CV_32F, ch), cv::Scalar::all(0));
	cv::Mat weights(h, w, CV_32F, cv::Scalar::all(0));

	for (auto &tile : tiles) {
		int x = tile.position.x;
		int y = tile.position.y;
		int xEnd = std::min(x + tileSize, w);
		int yEnd = std::min(y + tileSize, h);
		cv::Rect dstR(x, y, xEnd - x, yEnd - y);

		cv::Mat tf;
		tile.data.convertTo(tf, CV_32F);

		// build weight mask
		cv::Mat wm(dstR.height, dstR.width, CV_32F, 1.0f);
		if (overlap > 0) {
			// horizontal ramps
			cv::Mat rampH(1, overlap, CV_32F);
			for (int i = 0; i < overlap; ++i)
				rampH.at<float>(0, i) = overlap > 1 ? i / float(overlap - 1) : 1.f;
			cv::Mat rampH_rev = 1 - rampH;

			// vertical ramps
			cv::Mat rampV(overlap, 1, CV_32F);
			for (int i = 0; i < overlap; ++i)
				rampV.at<float>(i, 0) = overlap > 1 ? i / float(overlap - 1) : 1.f;
			cv::Mat rampV_rev = 1 - rampV;

			if (x > 0)
				wm.colRange(0, overlap).mul(cv::repeat(rampH, wm.rows, 1));
			if (xEnd < w)
				wm.colRange(wm.cols - overlap, wm.cols).mul(cv::repeat(rampH_rev, wm.rows, 1));
			if (y > 0)
				wm.rowRange(0, overlap).mul(cv::repeat(rampV.t(), 1, wm.cols));
			if (yEnd < h)
				wm.rowRange(wm.rows - overlap, wm.rows).mul(cv::repeat(rampV_rev.t(), 1, wm.cols));

			// boost border edges
			if (x == 0) {
				cv::Mat rampH2 = 0.5f + 0.5f * rampH;
				wm.colRange(0, overlap).mul(cv::repeat(rampH2, wm.rows, 1));
			}
			if (y == 0) {
				cv::Mat rampV2 = 0.5f + 0.5f * rampV;
				wm.rowRange(0, overlap).mul(cv::repeat(rampV2.t(), 1, wm.cols));
			}
		}

		// replicate mask across channels if needed
		cv::Mat wmC;
		if (ch > 1) {
			std::vector<cv::Mat> vcs(ch, wm);
			cv::merge(vcs, wmC);
		} else {
			wmC = wm;
		}

		// blend into result & accumulate weights
		result(dstR) += tf(cv::Rect(0, 0, dstR.width, dstR.height)).mul(wmC);
		weights(dstR) += wm;
	}

	// finalize: avoid div by zero
	cv::Mat zeroMask = (weights == 0);
	weights.setTo(1, zeroMask);
	cv::divide(result, weights, result); // auto-broadcast across channels

	return result;
}

