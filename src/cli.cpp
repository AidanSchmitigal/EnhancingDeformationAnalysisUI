#include <cli.hpp>

#include <stdio.h>
#include <string.h>

#include <core/stabilizer.hpp>
#include <core/CrackDetector.hpp>
#include <core/DenoiseInterface.hpp>
#include <core/FeatureTracker.hpp>
#include <core/ImageAnalysis.hpp>

#include <utils.h>

#include <filesystem>

bool LoadImages(const char* folder, std::vector<uint32_t*>& data, int& width, int& height) {
	// load the images
	if (!std::filesystem::exists(folder)) {
		return false;
	}

	// find all .tif files in the folder
	std::vector<std::string> files;
	for (const auto& entry : std::filesystem::directory_iterator(folder)) {
		if (entry.path().string().find(".tif") == std::string::npos)
			continue;
		files.push_back(entry.path().string());
	}

	// sort the files by name
	std::sort(files.begin(), files.end());
	for (int i = 0; i < files.size(); i++) {
		int w, h;
		uint32_t* img = utils::LoadTiff(files[i].c_str(), w, h);
		if (img == nullptr) {
			printf("Failed to load image %s\n", files[i].c_str());
			return false;
		}
		if (i == 0) {
			width = w;
			height = h;
		} else if (w != width || h != height) {
			printf("Image %s has different dimensions\n", files[i].c_str());
			return false;
		}
		data.push_back(img);
	}
	return true;
}

bool WriteAnalysis(const char* filename, std::vector<std::vector<float>>& histograms, std::vector<float>& avg_histogram, std::vector<float>& snrs, float avg_snr) {
	FILE* file = fopen(filename, "w");
	if (!file) {
		printf("Failed to open file %s (write analysis)\n", filename);
		return false;
	}
	for (int i = 0; i < histograms.size(); i++) {
		for (int j = 0; j < histograms[i].size(); j++) {
			fprintf(file, "%f,", histograms[i][j]);
		}
		fprintf(file, "%f\n", snrs[i]);
	}
	fprintf(file, "\n");
	for (int i = 0; i < avg_histogram.size(); i++) {
		fprintf(file, "%f,", avg_histogram[i]);
	}
	fprintf(file, "\n");
	fprintf(file, "%f\n", avg_snr);
	fclose(file);
	return true;
}

namespace cli {
	void run(int argc, char** argv) {
		const char* folder = NULL;
		const char* filter = "none"; // Default preprocessing filter
		const char* output = "results"; // Default output dir
		const char* stats_output = "results.csv";
		const char* widths_output = "widths.csv";
		int do_denoise = 0;
		int do_analyze = 0;
		int do_widths = 0;

		// Parse flags
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "--folder") == 0 && i + 1 < argc) {
				folder = argv[++i];
			} else if (strcmp(argv[i], "--crop") == 0 && i + 1 < argc) {
				filter = argv[++i];
			} else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
				output = argv[++i];
			} else if (strcmp(argv[i], "--denoise") == 0 && i + 1 < argc) {
				do_denoise = 1;
			} else if (strcmp(argv[i], "--analyze") == 0 && i + 1 < argc) {
				stats_output = argv[++i];
				do_analyze = 1;
			} else if (strcmp(argv[i], "--calculate-widths") == 0 && i + 1 < argc) {
				widths_output = argv[++i];
				do_widths = 1;
			} else if (strcmp(argv[i], "--help") == 0) {
				printf("Usage: %s --folder <path> [--crop <pixels_from_bottom_to_remove>] [--denoise <blur/sfr_hrsem/sfr_lrsem>] [--analyze <output.csv>] [--calculate-widths <widths.csv>] [--output <path_to_folder>]\n", argv[0]);
				return;
			} else {
				printf("Unknown flag: %s\n", argv[i]);
			}
		}

		// Validate required input
		if (!folder) {
			printf("Error: --folder is required\n");
			printf("Usage: %s --folder <path> [--crop <pixels_from_bottom_to_remove>] [--denoise <blur/sfr_hrsem/sfr_lrsem>] [--analyze <output.csv>] [--output <path_to_folder>]\n", argv[0]);
		}
		
		// validate filename and load images
		std::vector<uint32_t*> images;
		int width, height;
		bool success = LoadImages(folder, images, width, height);
		if (!success) {
			printf("Failed to load images from %s\n", folder);
			return;
		}

		// validate filter
		const char* models[] = { "blur", "sfr_hrsem", "sfr_hrstem", "sfr_hrtem", "sfr_lrsem", "sfr_lrstem", "sfr_lrtem"};
		if (do_denoise) {
			bool found = false;
			for (int i = 0; i < 6; i++) {
				if (strcmp(filter, models[i]) == 0) {
					found = true;
					break;
				}
			}
			if (!found) {
				printf("Invalid denoise filter: %s\n", filter);
				printf("These are the available filters: blur, sfr_hrsem, sfr_hrstem, sfr_hrtem, sfr_lrsem, sfr_lrstem, sfr_lrtem\n");
				return;
			}
		}

		// Execute based on flags
		if (do_denoise) {
			DenoiseInterface::DenoiseNew(images, width, height, filter, 256, 10);
		}
		if (do_analyze) {
			std::vector<std::vector<float>> histograms;
			std::vector<float> avg_histogram;
			std::vector<float> snrs;
			float avg_snr;
			ImageAnalysis::AnalyzeImages(images, width, height, histograms, avg_histogram, snrs, avg_snr);
			
		}
		if (!do_denoise && !do_analyze && !do_widths) {
			printf("Nothing to do. Use --preprocess and/or --analyze and/or --do-widths.\n");
		}
	}
}
