#include <string>

namespace utils {
	// TODO: fill in the filter for win32 (although it may not matter?)
	std::string OpenFileDialog(const char* open_path = ".", const char* title = "", const bool folders_only = false, const char* filter = "");

	// Load a tiff image
	// remember to free this pointer
	unsigned int* LoadTiff(const char* path, int& width, int& height);

	bool WriteTiff(const char* path, unsigned int* data, int width, int height);
}
