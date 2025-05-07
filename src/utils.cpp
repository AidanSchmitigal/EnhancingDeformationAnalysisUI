#include <utils.h>

#include <string.h>
#include <stdio.h>
#include <fstream>

#include <tiffio.h>
#include <opencv2/opencv.hpp>
#include <gif-h/gif.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
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
		// if no filename was returned, return an empty string
		// if we don't do this check we get garbage data into the string when the user cancels the dialog
		if (out == nullptr) return std::string();
		output[strcspn(output, "\n")] = 0;
		if (output[0] == 0)
			return std::string();
		else
			return std::string(output);
#endif
	}

	std::string SaveFileDialog(const char* save_path, const char* title, const char* filter) {
#ifdef _WIN32
		CoInitialize(nullptr);
		IFileDialog* pFileDialog = nullptr;
		std::wstring folderPath;
		if (SUCCEEDED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_IFileDialog, reinterpret_cast<void**>(&pFileDialog))))
		{
			DWORD dwOptions;
			if (SUCCEEDED(pFileDialog->GetOptions(&dwOptions)))
			{
				pFileDialog->SetOptions(dwOptions | FOS_OVERWRITEPROMPT);
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
		char buf[256];
		snprintf(buf, 256, "zenity --file-selection --save --title=\"%s\" --filename=%s", title, save_path);
		char output[1024];
		FILE *f = popen(buf, "r");
		auto out = fgets(output, 1024, f);
		// if we don't do this check we get garbage data into the string when the user cancels the dialog
		if (out == nullptr) return std::string();
		output[strcspn(output, "\n")] = 0;
		if (output[0] == 0)
			return std::string();
		else
			return std::string(output);
#endif
	}

	unsigned int* LoadTiff(const char* path, int& width, int& height) {
		PROFILE_FUNCTION();

		// set the warning handler to null to avoid printing warnings
		// errors are still printed
		TIFFSetWarningHandler(nullptr);
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

	bool LoadTiffFolder(const char* folder_path, std::vector<uint32_t*>& images, int& width, int& height) {
		PROFILE_FUNCTION();

		if (!std::filesystem::exists(folder_path)) {
			printf("Path does not exist\n");
			return false;
		}

		// find all .tif files in the folder
		std::vector<std::string> files;
		for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
			if (entry.path().string().find(".tif") == std::string::npos)
				continue;
			files.push_back(entry.path().string());
		}

		// sort the files by name
		std::sort(files.begin(), files.end());
		for (const auto& file : files) {
			PROFILE_SCOPE(LoadTiffFolderLoop);

			uint32_t* temp = utils::LoadTiff(file.c_str(), width, height);
			if (!temp) {
				printf("Could not load file %s\n", file.c_str());
				return false;
			}
			
			images.push_back(temp);
		}
		return true;
	}

	bool WriteTiff(const char* path, unsigned int* data, int width, int height) {
		PROFILE_FUNCTION()

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

	// write gif using gif.h
	bool WriteGIFOfImageSet(const char* path, std::vector<Texture*> images, int delay, int loop) {
		PROFILE_FUNCTION()

			if (images.empty()) {
				printf("No images to write to gif\n");
				return false;
			}
		// Create gif
		GifWriter writer;
		if (!GifBegin(&writer, path, images[0]->GetWidth(), images[0]->GetHeight(), delay, loop))
			return false;

		// all textures are the same size, so allocate once and then replace each time
		uint32_t* data = (uint32_t*)malloc(images[0]->GetWidth() * images[0]->GetHeight() * 4);
		for (int i = 0; i < images.size(); i++) {
			images[i]->GetData(data);
			GifWriteFrame(&writer, (uint8_t*)data, images[i]->GetWidth(), images[i]->GetHeight(), delay);
		}
		free(data);
		// Close gif
		GifEnd(&writer);
		return true;
	}

	void GetDataFromTexture(unsigned int* data, int width, int height, Texture* texture) {
		texture->GetData(data);
	}

	void GetDataFromTextures(std::vector<uint32_t*>& data, int width, int height, std::vector<Texture*>& textures) {
		PROFILE_FUNCTION()

			if (data.size() != textures.size()) data.resize(textures.size());
		for (int i = 0; i < textures.size(); i++) {
			if (data[i] == nullptr) data[i] = (uint32_t*)malloc(width * height * 4);
			textures[i]->GetData(data[i]);
		}
	}

	void LoadDataIntoTexturesAndFree(std::vector<Texture*>& textures, std::vector<uint32_t*>& data, int width, int height) {
		PROFILE_FUNCTION()

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

	bool WriteCSV(const char* path, std::vector<std::vector<cv::Point2f>>& trackedPts, std::vector<std::vector<float>>& widths) {
		std::ofstream f(path);
		if (!f.is_open()) {
			std::cerr << "Could not open file " << path << std::endl;
			return false;
		}
		// header
		f << "frame";
		auto nPairs = widths.empty() ? 0 : widths[0].size();
		for (size_t p = 0; p < nPairs; ++p) f << ",width" << p;
		auto nPts = trackedPts.empty() ? 0 : trackedPts[0].size();
		for (size_t j = 0; j < nPts; ++j)
			f << ",pt" << j << "_x,pt" << j << "_y";
		f << "\n";

		auto nFrames = std::min(widths.size(), trackedPts.size());
		for (size_t i = 0; i < nFrames; ++i) {
			f << i;
			for (auto w : widths[i]) f << "," << w;
			for (auto& pt : trackedPts[i]) f << "," << pt.x << "," << pt.y;
			f << "\n";
		}
		return true;
	}

	bool saveAnalysisCsv(const char* path, const std::vector<std::vector<float>>& histograms, const std::vector<float>& avg_histogram, const std::vector<float>& snrs, float avg_snr) {
		std::ofstream f(path);
		if (!f.is_open()) {
			std::cerr << "Could not open file " << path << std::endl;
			return false;
		}
		int bins = avg_histogram.size();

		// header
		f << "frame,snr";
		for (int b = 0; b < bins; ++b)
			f << ",bin" << b;
		f << "\n";

		// per-frame rows
		size_t n = std::min(histograms.size(), snrs.size());
		for (size_t i = 0; i < n; ++i) {
			f << i << "," << snrs[i];
			for (int b = 0; b < bins; ++b)
				f << "," << histograms[i][b];
			f << "\n";
		}

		// average row
		f << "avg," << avg_snr;
		for (int b = 0; b < bins; ++b)
			f << "," << avg_histogram[b];
		f << "\n";
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

	bool DirectoryContainsTiff(const std::filesystem::path& path)
	{
		for (auto& it : std::filesystem::directory_iterator(path))
			if (it.path().string().find(".tif") != std::string::npos)
				return true;
		return false;
	}
}

Profiler::Profiler(const char* name) {
	start_time = std::chrono::high_resolution_clock::now();
	this->name = name;
}

Profiler::~Profiler() {
	if (!stopped)
		Stop();
}

void Profiler::Stop() {
	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
	printf("Profiler: %s took %zu ms\n", name, duration);
	stopped = true;
}
