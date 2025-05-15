#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <OpenGL/Texture.h>
#include <core/Tiler.hpp>

#include <opencv2/opencv.hpp>

namespace utils {
// TODO: fill in the filter for win32 (although it may not matter?)
std::string OpenFileDialog(const char *open_path = ".", const char *title = "",
			   const bool folders_only = false,
			   const char *filter = "");
std::string SaveFileDialog(const char *save_path = ".", const char *title = "",
			   const char *filter = "");

// gets the data from a texture
// assumes data is already allocated
void GetDataFromTexture(unsigned int *data, std::shared_ptr<Texture> texture,
			int width = 0, int height = 0);

// gets the data from a vector of textures
// doesn't assume data is already allocated and will allocate if not
void GetDataFromTextures(std::vector<uint32_t *> &data, int width, int height,
			 std::vector<std::shared_ptr<Texture>> &textures);

// loads data into textures and frees the data
// basically a helper function to consolidate a lot of the code in ImageSet.cpp
void LoadDataIntoTexturesAndFree(
    std::vector<std::shared_ptr<Texture>> &textures,
    std::vector<uint32_t *> &data, int width, int height);

bool DirectoryContainsTiff(const std::filesystem::path &path);
} // namespace utils

namespace io {
uint32_t *LoadTiff(const char *path, int &width, int &height);

bool WriteTiff(const char *path, unsigned int *data, int width, int height);

bool LoadTiffFolder(const char *folder_path, std::vector<uint32_t *> &images,
		    int &width, int &height);

bool WriteGIFOfImageSet(const char *path,
			std::vector<std::shared_ptr<Texture>> images,
			int delay = 100, int loop = 0);

bool WriteCSV(const char *path,
	      std::vector<std::vector<std::vector<float>>> &data);
bool WriteCSV(const char *path, std::vector<std::vector<cv::Point2f>> &points,
	      std::vector<std::vector<float>> &data);

bool SaveAnalysisCsv(const char *path,
		     const std::vector<std::vector<float>> &histograms,
		     const std::vector<float> &avg_histogram,
		     const std::vector<float> &snrs, float avg_snr);
} // namespace io

class Profiler {
      public:
	Profiler(const char *name = "'empty'");
	~Profiler();
	void Stop();

      private:
	const char *name;
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
