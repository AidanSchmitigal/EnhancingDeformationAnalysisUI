#include <string>

namespace utils {
	// TODO: fill in the filter for win32
	std::string OpenFileDialog(const char* open_path = ".", const bool folders_only = false, const char* filter = "");
}
