#include <utils.h>

#include <string.h>
#include <stdio.h>
#include <fstream>

#include <tiffio.h>
#include <opencv2/opencv.hpp>

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#endif

namespace utils {
	std::string OpenFileDialog(const char* open_path, const char* title, const bool folders_only, const char* filter) {
#ifdef _WIN32
		CoInitialize(nullptr);
		IFileDialog* pFileDialog = nullptr;
		std::wstring folderPath;

		if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileDialog, reinterpret_cast<void**>(&pFileDialog))))
		{
			DWORD dwOptions;
			if (SUCCEEDED(pFileDialog->GetOptions(&dwOptions)))
			{
				pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS);
			}

			if (SUCCEEDED(pFileDialog->Show(nullptr)))
			{
				IShellItem* pItem;
				if (SUCCEEDED(pFileDialog->GetResult(&pItem)))
				{
					PWSTR pszFilePath;
					if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)))
					{
						folderPath = pszFilePath;
						CoTaskMemFree(pszFilePath);
					}
					pItem->Release();
				}
			}
			pFileDialog->Release();
		}

		CoUninitialize();
		if (folderPath.empty()) return std::string();

		int size_needed = WideCharToMultiByte(CP_UTF8, 0, folderPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string str(size_needed - 1, 0);
		WideCharToMultiByte(CP_UTF8, 0, folderPath.c_str(), -1, &str[0], size_needed, nullptr, nullptr);
		return str;
#else

		// Set zenity to open in our current directory
		char buf[256];
		if (folders_only)
			snprintf(buf, 256, "zenity --file-selection --title=\"%s\" --directory --filename=%s/", title, open_path);
		else
			snprintf(buf, 256, "zenity --file-selection --title=\"%s\" --filename=%s/", title, open_path);

		char output[1024];

		// open the zenity window
		FILE *f = popen(buf, "r");

		// get filename from zenity
		auto out = fgets(output, 1024, f);

		// remove any newlines
		output[strcspn(output, "\n")] = 0;

		if (output[0] == 0)
			return std::string();
		else
			return std::string(output);
#endif
	}

	unsigned int* LoadTiff(const char* path, int& width, int& height) {
		TIFF* tif = TIFFOpen(path, "r");
		if (!tif) {
			printf("Could not open file %s\n", path);
			return NULL;
		}
		size_t npixels;
		uint32_t* raster;
		size_t samplesperpixel;

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
		TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);

		TIFFRGBAImage img;
		char emsg[1024];
		if (!TIFFRGBAImageBegin(&img, tif, 0, emsg)) {
			TIFFError(path, "%s", emsg);
			TIFFClose(tif);
			return NULL;
		}
		npixels = width * height;
		raster = (uint32_t*)_TIFFmalloc(npixels * sizeof(uint32_t));
		if (raster) {
			if (TIFFRGBAImageGet(&img, raster, width, height)) {
				// flip the image
				uint32_t* temp = (uint32_t*)_TIFFmalloc(npixels * sizeof(uint32_t));
				for (int i = 0; i < height; i++) {
					memcpy(temp + i * width, raster + (height - i - 1) * width, width * sizeof(uint32_t));
				}
				TIFFRGBAImageEnd(&img);
				_TIFFfree(raster);
				TIFFClose(tif);
				return temp;
			}
		}
		TIFFRGBAImageEnd(&img);
		_TIFFfree(raster);
		TIFFClose(tif);
		return NULL;
	}

	bool WriteTiff(const char* path, unsigned int* data, int width, int height) {
		TIFF* tif = TIFFOpen(path, "w");
		if (!tif) {
			printf("Could not open file %s\n", path);
			return false;
		}

		TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
		TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
		TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
		TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
		TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
		TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
		TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);

		for (int i = 0; i < height; i++) {
			if (TIFFWriteScanline(tif, data + i * width, i, 0) < 0) {
				printf("Error writing tiff\n");
				TIFFClose(tif);
				return false;
			}
		}

		TIFFClose(tif);
		return true;
	}

	void GetDataFromTexture(unsigned int* data, int width, int height, Texture* texture) {
		texture->GetData(data);
	}

	void GetDataFromTextures(std::vector<uint32_t*>& data, int width, int height, std::vector<Texture*>& textures) {
		if (data.size() != textures.size()) data.resize(textures.size());
		for (int i = 0; i < textures.size(); i++) {
			if (data[i] == nullptr) data[i] = (uint32_t*)malloc(width * height * 4);
			textures[i]->GetData(data[i]);
		}
	}

	void LoadDataIntoTexturesAndFree(std::vector<Texture*>& textures, std::vector<uint32_t*>& data, int width, int height) {
		for (int i = 0; i < textures.size(); i++) {
			textures[i]->Load(data[i], width, height);
			free(data[i]);
		}
	}

	bool WriteCSV(const char* path, std::vector<std::vector<std::vector<float>>>& data) {
		FILE* f = fopen(path, "w");
		if (!f) {
			printf("Could not open file %s\n", path);
			return false;
		}
		for (int i = 0; i < data.size(); i++) {
			for (int j = 0; j < data[i][0].size(); j++) {
				fprintf(f, "%f", data[i][0][j]);
				if (j < data[i][0].size() - 1) fprintf(f, ",");
			}
			fprintf(f, "\n");
		}
		fclose(f);
		return true;
	}

	bool WriteCSV(const char* path, std::vector<cv::Point2f>& points, std::vector<std::vector<float>>& data) {
		std::ofstream file = std::ofstream(path);

		if (!file.is_open()) {
			return false;
		}

		// Write header
		file << "Point_Pair,Point1_X,Point1_Y,Point2_X,Point2_Y";
		for (size_t i = 0; i < data.size(); ++i) {
			file << ",Frame_" << i;
		}
		file << "\n";

		// Write data - assuming points are pairs and widths are per frame
		for (size_t i = 0; i < points.size() / 2; ++i) {
			file << i << "," 
				<< points[2*i].x << "," << points[2*i].y << "," 
				<< points[2*i+1].x << "," << points[2*i+1].y;
			
			for (size_t j = 0; j < data.size(); ++j) {
				file << "," << data[j][i];
			}
			file << "\n";
		}

		file.close();
		return true;
	}

	std::vector<ImageTile> splitImageIntoTiles(const cv::Mat& image, int tileSize, int overlap) {
		std::vector<ImageTile> tiles;

		// Calculate padded dimensions
		int step = tileSize - overlap;
		int paddedWidth = ceil((float)image.cols / step) * step + overlap;
		int paddedHeight = ceil((float)image.rows / step) * step + overlap;

		// Pad image with zeros
		cv::Mat paddedImage;
		cv::copyMakeBorder(image, paddedImage, 0, paddedHeight - image.rows, 
				0, paddedWidth - image.cols, cv::BORDER_CONSTANT, cv::Scalar(1.0f));

		// Split into tiles with overlap
		for (int y = 0; y <= paddedImage.rows - tileSize; y += step) {
			for (int x = 0; x <= paddedImage.cols - tileSize; x += step) {
				cv::Rect roi(x, y, tileSize, tileSize);
				cv::Mat tile = paddedImage(roi).clone();
				tiles.push_back({tile, cv::Point(x, y), cv::Size(tileSize, tileSize)});
			}
		}

		return tiles;
	}

	cv::Mat reconstructImageFromTiles(const std::vector<ImageTile>& tiles, cv::Size originalSize, int overlap) {
		if (tiles.empty()) {
			std::cerr << "No tiles provided!" << std::endl;
			return cv::Mat();
		}

		// Determine padded size from tiles
		int maxX = 0, maxY = 0;
		for (const auto& tile : tiles) {
			maxX = std::max(maxX, tile.position.x + tile.size.width);
			maxY = std::max(maxY, tile.position.y + tile.size.height);
		}
		cv::Size paddedSize(maxX, maxY);

		// Initialize accumulation and weight matrices
		cv::Mat reconstructed = cv::Mat::zeros(paddedSize, CV_32F); // Float for accumulation
		cv::Mat weight = cv::Mat::zeros(paddedSize, CV_32F);
		int tileSize = tiles[0].size.width;

		// Create linear blend mask for overlap
		cv::Mat blendMask = cv::Mat::ones(tileSize, tileSize, CV_32F);
		if (overlap > 0) {
			for (int y = 0; y < tileSize; ++y) {
				for (int x = 0; x < tileSize; ++x) {
					float wx = (x < overlap) ? (x / (float)overlap) : 
						(x >= tileSize - overlap ? (tileSize - x - 1) / (float)overlap : 1.0f);
					float wy = (y < overlap) ? (y / (float)overlap) : 
						(y >= tileSize - overlap ? (tileSize - y - 1) / (float)overlap : 1.0f);
					blendMask.at<float>(y, x) = wx * wy;
				}
			}
		}

		// Blend tiles
		for (const auto& tile : tiles) {
			cv::Rect roi(tile.position, tile.size);

			cv::Mat weightedTile;
			cv::multiply(tile.data, blendMask, weightedTile);
			reconstructed(roi) += weightedTile;
			weight(roi) += blendMask;
		}

		// Normalize and convert back to grayscale
		cv::Mat normalized;
		cv::divide(reconstructed, weight, normalized); // Handle division by zero implicitly

		// Crop to original size
		return normalized(cv::Rect(0, 0, originalSize.width, originalSize.height));
	}
}
