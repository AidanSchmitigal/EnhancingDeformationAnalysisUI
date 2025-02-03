#include <utils.h>

#include <string.h>
#include <stdio.h>

#include <tiffio.h>

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#endif

namespace utils {
	std::string OpenFileDialog(const char* open_path, const bool folders_only, const char* filter) {
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

	unsigned int* LoadTiff(const char* path, int& width, int& height) {
		TIFF* tif = TIFFOpen(path, "r");
		if (!tif) {
			printf("Could not open file %s\n", path);
			return NULL;
		}
		size_t npixels;
		uint32_t* raster;

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

		TIFFRGBAImage img;
		char emsg[1024];
		if (!TIFFRGBAImageBegin(&img, tif, 0, emsg)) {
			TIFFError(path, emsg);
			TIFFClose(tif);
			return NULL;
		}
		npixels = width * height;
		raster = (uint32_t*)_TIFFmalloc(npixels * sizeof(uint32_t));
		if (raster) {
			if (TIFFRGBAImageGet(&img, raster, width, height)) {
				printf("Loaded %s, %d x %d\n", path, width, height);
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
}
