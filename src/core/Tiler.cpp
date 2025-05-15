#include <core/Tiler.hpp>

std::vector<Tile> Tiler::CreateTiles(const cv::Mat &image, const TileConfig &config) {
	if (config.type == TileType::Cropped) {
		return CreateCroppedTiles(image, config);
	} else if (config.type == TileType::Blended) {
		return CreateBlendedTiles(image, config);
	}
	return {};
}

cv::Mat Tiler::StitchTiles(const std::vector<Tile>& tiles, const TileConfig& config, const cv::Size& originalSize, bool singleChannel) {
	if (singleChannel) {
		return StitchCroppedTilesSingleChannel(tiles, originalSize, config);
	} else if (config.type == TileType::Cropped) {
		return StitchCroppedTiles(tiles, originalSize, config);
	} else if (config.type == TileType::Blended) {
		return StitchBlendedTiles(tiles, originalSize, config);
	}
	return {};
}

std::vector<Tile> Tiler::CreateCroppedTiles(const cv::Mat& image, const TileConfig& config) {
	auto tileSize = config.tileSize;
	auto centerSize = config.centerSize;
	auto includeOutside = config.includeOutside;

	int height = image.rows;
	int width  = image.cols;
	int centerInset = (tileSize - centerSize) / 2;
	std::vector<Tile> tiles;

	int y0 = includeOutside ? -centerInset : 0;
	for (int y = y0; y < height; y += centerSize) {
		for (int x = y0; x < width; x += centerSize) {
			int yStart = std::max(y, 0);
			int xStart = std::max(x, 0);
			int yEnd   = std::min(y + tileSize, height);
			int xEnd   = std::min(x + tileSize, width);

			int shiftY = yStart - y;
			int shiftX = xStart - x;

			cv::Mat roi = image(cv::Range(yStart, yEnd), cv::Range(xStart, xEnd));
			cv::Mat tile;
			if (roi.rows  != tileSize || roi.cols != tileSize) {
				tile = cv::Mat::zeros(tileSize, tileSize, image.type());
				roi.copyTo(tile(cv::Rect(shiftX, shiftY, roi.cols, roi.rows)));
			} else {
				tile = roi.clone();
			}

			tiles.push_back({ tile, cv::Point(x, y) });
		}
	}
	return tiles;

}

std::vector<Tile> Tiler::CreateBlendedTiles(const cv::Mat& image, const TileConfig& config) {
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
			tiles.push_back({ tile, cv::Point(x, y) });
		}
	}
	return tiles;
}

cv::Mat Tiler::StitchCroppedTiles(const std::vector<Tile>& tiles, const cv::Size& originalSize, const TileConfig& config) {
	auto tileSize = config.tileSize;
	auto centerSize = config.centerSize;
	auto includeOutside = config.includeOutside;

	int height = originalSize.height;
	int width  = originalSize.width;
	int centerInset = (tileSize - centerSize) / 2;

	// 2-channel float result; adjust type/channels if needed
	cv::Mat result(height, width, CV_32FC2, cv::Scalar::all(0));

	for (auto& tile : tiles) {
		int x = tile.position.x;
		int y = tile.position.y;

		int cx0 = std::max(x + centerInset, 0);
		int cy0 = std::max(y + centerInset, 0);
		int cx1 = std::min(x + tileSize - centerInset, width);
		int cy1 = std::min(y + tileSize - centerInset, height);

		int tx0 = centerInset;
		int ty0 = centerInset;
		int tx1 = cx1 - x;
		int ty1 = cy1 - y;

		// edge adjustments when not including outside
		if (!includeOutside) {
			if (y < centerSize)      { cy0 = 0;        ty0 = 0; }
			if (y + tileSize > height) { cy1 = height;  ty1 = std::min(tileSize, height - y); }
			if (x < centerSize)      { cx0 = 0;        tx0 = 0; }
			if (x + tileSize > width)  { cx1 = width;   tx1 = std::min(tileSize, width - x); }
		}

		cv::Mat srcROI = tile.data(cv::Range(ty0, ty1), cv::Range(tx0, tx1));
		cv::Mat dstROI = result(cv::Range(cy0, cy1), cv::Range(cx0, cx1));
		srcROI.copyTo(dstROI);
	}

	return result;

}

cv::Mat Tiler::StitchCroppedTilesSingleChannel(const std::vector<Tile>& tiles, const cv::Size& originalSize, const TileConfig& config) {
	auto tileSize = config.tileSize;
	auto centerSize = config.centerSize;
	auto includeOutside = config.includeOutside;

	int h = originalSize.height;
	int w = originalSize.width;
	int inset = (tileSize - centerSize) / 2;
	cv::Mat result(h, w, CV_32F, 0.0f);

	for (const auto& tile : tiles) {
		int x = tile.position.x;
		int y = tile.position.y;

		// compute dest bounds
		int cx0 = std::max(x + inset, 0);
		int cy0 = std::max(y + inset, 0);
		int cx1 = std::min(x + tileSize - inset, w);
		int cy1 = std::min(y + tileSize - inset, h);

		if (!includeOutside) {
			if (y < centerSize)          { cy0 = 0; }
			if (y + tileSize > h)        { cy1 = h; }
			if (x < centerSize)          { cx0 = 0; }
			if (x + tileSize > w)        { cx1 = w; }
		}

		// width/height of the region to copy
		int copyW = cx1 - cx0;
		int copyH = cy1 - cy0;
		if (copyW <= 0 || copyH <= 0) continue;

		// compute src offsets
		int tx0 = inset + (cx0 - (x + inset));
		int ty0 = inset + (cy0 - (y + inset));

		// clamp src offsets
		tx0 = std::clamp(tx0, 0, tileSize-1);
		ty0 = std::clamp(ty0, 0, tileSize-1);
		// ensure srcRect fits
		if (tx0 + copyW > tileSize) copyW = tileSize - tx0;
		if (ty0 + copyH > tileSize) copyH = tileSize - ty0;
		if (copyW <= 0 || copyH <= 0) continue;

		cv::Mat srcROI = tile.data(cv::Rect(tx0, ty0, copyW, copyH));
		cv::Mat dstROI = result(cv::Rect(cx0, cy0, copyW, copyH));
		srcROI.copyTo(dstROI);
	}

	return result;
}

cv::Mat Tiler::StitchBlendedTiles(const std::vector<Tile>& tiles, const cv::Size& originalSize, const TileConfig& config) {
	auto tileSize = config.tileSize;
	auto overlap = config.overlap;
	int w = originalSize.width, h = originalSize.height;
	int ch = tiles.empty() ? 1 : tiles[0].data.channels();
	cv::Mat result(h, w, CV_MAKETYPE(CV_32F, ch), cv::Scalar::all(0));
	cv::Mat weights(h, w, CV_32F, cv::Scalar::all(0));

	for (auto& tile : tiles) {
		int x = tile.position.x, y = tile.position.y;
		int xEnd = std::min(x + tileSize, w);
		int yEnd = std::min(y + tileSize, h);
		cv::Rect dstR(x, y, xEnd - x, yEnd - y);

		cv::Mat tf;
		tile.data.convertTo(tf, CV_32F);

		// make weight mask
		cv::Mat wm(dstR.height, dstR.width, CV_32F, cv::Scalar::all(1));
		if (overlap > 0) {
			// horizontal ramp
			cv::Mat rampH(1, overlap, CV_32F), rampH_rev, rampH2;
			for (int i = 0; i < overlap; ++i) {
				float r = overlap>1 ? i/float(overlap-1) : 1.f;
				rampH.at<float>(0,i) = r;
			}
			rampH_rev = 1 - rampH;
			// vertical ramp
			cv::Mat rampV(overlap, 1, CV_32F), rampV_rev, rampV2;
			for (int i = 0; i < overlap; ++i) {
				float r = overlap>1 ? i/float(overlap-1) : 1.f;
				rampV.at<float>(i,0) = r;
			}
			rampV_rev = 1 - rampV;
			// apply feathering
			if (x>0) wm.colRange(0, overlap).mul(cv::repeat(rampH, wm.rows, 1));
			if (xEnd<w) wm.colRange(wm.cols-overlap, wm.cols).mul(
					cv::repeat(rampH_rev, wm.rows,1));
			if (y>0) wm.rowRange(0, overlap).mul(
					cv::repeat(rampV.t(), 1, wm.cols));
			if (yEnd<h) wm.rowRange(wm.rows-overlap, wm.rows).mul(
					cv::repeat(rampV_rev.t(), 1, wm.cols));
			// boost edges at image border
			if (x==0) {
				rampH2 = 0.5f + 0.5f*rampH; 
				wm.colRange(0, overlap).mul(cv::repeat(rampH2, wm.rows,1));
			}
			if (y==0) {
				rampV2 = 0.5f + 0.5f*rampV;
				wm.rowRange(0, overlap).mul(
						cv::repeat(rampV2.t(), 1, wm.cols));
			}
		}

		// replicate mask across channels
		cv::Mat wmC;
		if (ch>1) {
			std::vector<cv::Mat> vcs(ch, wm);
			cv::merge(vcs, wmC);
		} else wmC = wm;

		// blend
		result(dstR) += tf(cv::Rect(0,0,dstR.width,dstR.height)).mul(wmC);
		weights(dstR)  += wm;
	}

	// avoid zero-div
	cv::Mat zeroM = (weights == 0);
	weights.setTo(1, zeroM);
	cv::divide(result, weights, result); // broadcasts over channels
	return result;
}
