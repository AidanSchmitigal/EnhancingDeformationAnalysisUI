#include <utils.h>

#include <string.h>
#include <stdio.h>

#include <tiffio.h>

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
}
