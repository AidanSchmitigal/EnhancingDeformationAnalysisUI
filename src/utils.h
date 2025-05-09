#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

#include <OpenGL/Texture.h>

#include <opencv2/opencv.hpp>

// this is here because it's used for the splitImageIntoTiles and reconstructImageFromTiles functions
// probably should be moved to a different file but idk
// Structure to hold tile information
struct ImageTile {
	cv::Mat data;       // The processed tile
	cv::Point position; // Top-left position in original image
	cv::Size size;      // Size of the tile
};

namespace utils {
	// TODO: fill in the filter for win32 (although it may not matter?)
	std::string OpenFileDialog(const char* open_path = ".", const char* title = "", const bool folders_only = false, const char* filter = "");
	std::string SaveFileDialog(const char* save_path = ".", const char* title = "", const char* filter = "");

	// Load a tiff image
	// remember to free this pointer
	uint32_t* LoadTiff(const char* path, int& width, int& height);

	// assumes the images are all the same size, generally true for SEM imagery
	bool LoadTiffFolder(const char* folder_path, std::vector<uint32_t*>& images, int& width, int& height);

	// writes a tiff image
	bool WriteTiff(const char* path, unsigned int* data, int width, int height);

	// writes a gif of a vector of textures
	bool WriteGIFOfImageSet(const char* path, std::vector<std::shared_ptr<Texture>> images, int delay = 100, int loop = 0);

	// gets the data from a texture
	// assumes data is already allocated
	void GetDataFromTexture(unsigned int* data, int width, int height, std::shared_ptr<Texture> texture);

	// gets the data from a vector of textures
	// doesn't assume data is already allocated and will allocate if not
	void GetDataFromTextures(std::vector<uint32_t*>& data, int width, int height, std::vector<std::shared_ptr<Texture>>& textures);

	// loads data into textures and frees the data
	// basically a helper function to consolidate a lot of the code in ImageSet.cpp
	void LoadDataIntoTexturesAndFree(std::vector<std::shared_ptr<Texture>>& textures, std::vector<uint32_t*>& data, int width, int height);

	// writes our auto width tracking data to a csv
	bool WriteCSV(const char* path, std::vector<std::vector<std::vector<float>>>& data);

	// writes our manual width tracking data to a csv
	bool WriteCSV(const char* path, std::vector<std::vector<cv::Point2f>>& points, std::vector<std::vector<float>>& data);

	bool saveAnalysisCsv(const char* path, const std::vector<std::vector<float>>& histograms, const std::vector<float>& avg_histogram, const std::vector<float>& snrs, float avg_snr);

	// functions to split an image into tiles and then reconstruct it
	// used for denoising
	std::vector<ImageTile> splitImageIntoTiles(const cv::Mat& image, int tileSize, int overlap = 0);
	cv::Mat reconstructImageFromTiles(const std::vector<ImageTile>& tiles, cv::Size originalSize, int overlap = 0);

	bool DirectoryContainsTiff(const std::filesystem::path& path);
}

class Profiler {
	public:
		Profiler(const char* name = "'empty'");
		~Profiler();
		void Stop();
	private:
		const char* name;
		std::chrono::high_resolution_clock::time_point start_time;
		bool stopped = false;
};

#ifdef UI_PROFILE
	#define PROFILE_FUNCTION(name) Profiler profiler_##name(__FUNCTION__);
	#define PROFILE_SCOPE(name) Profiler profiler_##name(#name);
#else
	#define PROFILE_FUNCTION(name)
	#define PROFILE_SCOPE(name)
#endif
