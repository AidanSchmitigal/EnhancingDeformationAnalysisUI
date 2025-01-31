#include <utils.h>

#include <string.h>
#include <stdio.h>

namespace utils {
	std::string OpenFileDialog(const char* open_path, const bool folders_only, const char* filter) {
#ifdef _WIN32
		OPENFILENAMEA ofn;      // common dialog box structure
		CHAR szFile[260] = {0}; // if using TCHAR macros

		// Initialize OPENFILENAME (memset to zero)
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE) {
			std::string fp = ofn.lpstrFile;
			for (size_t i = 0; i < fp.size(); i++) {
				if (fp[i] == '\\')
					fp[i] = '/';
			}
			// std::replace(fp.begin(), fp.end(), '\\', '/');
			return fp;
		}

		return std::string();
#else

		// Set zenity to open in our current directory
		char buf[256];
		if (!folders_only)
			snprintf(buf, 256, "zenity --file-selection --title=\"Choose an Image Folder\" --directory --filename=%s/", open_path);
		else
			snprintf(buf, 256, "zenity --file-selection --title=\"Choose an Image Folder\" --filename=%s/", open_path);

		char output[1024];

		// open the zenity window
		FILE *f = popen(buf, "r");

		// get filename from zenity
		fgets(output, 1024, f);

		// remove any newlines
		output[strcspn(output, "\n")] = 0;

		if (output[0] == 0)
			return std::string();
		else
			return std::string(output);
#endif
	}
}
