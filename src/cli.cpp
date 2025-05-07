#include <cli.hpp>

#include <stdio.h>
#include <string.h>

#include <core/Stabilizer.hpp>
#include <core/CrackDetector.hpp>
#include <core/DenoiseInterface.hpp>
#include <core/FeatureTracker.hpp>
#include <core/ImageAnalysis.hpp>

#include <utils.h>

#include <filesystem>

// Settings struct
struct Settings
{
	std::string folder;
	int crop_pixels = 0;
	bool do_crop = false;
	std::string filter;
	int denoise_tile_size = 0;
	bool do_denoise = false;
	std::string stats_output;
	bool do_analyze = false;
	std::string widths_output;
	bool do_widths = false;
	std::string output;
};

// Helper to check if a string is a flag
bool is_flag(const std::string &arg)
{
	return arg.size() >= 2 && arg[0] == '-' && arg[1] == '-';
}

// Helper to throw errors with usage hint
void print_usage_error(const char *flag, const char *msg, const char *prog_name)
{
	std::cerr <<
		std::string(msg) + " '" + flag + "'\n" +
		"Usage: " + prog_name + " --folder <path> [--crop <pixels>] [--denoise <blur/sfr_hrsem/sfr_lrsem>] [--analyze <output.csv>] [--calculate-widths <widths.csv>] [--output <folder_path>]\n";
	exit(1);
}

// Parse flags with argument count validation
Settings parse_flags(int argc, char *argv[])
{
	Settings settings;

	// Map of flag to {handler, expected_arg_count}
	// arg_count: 0 for no args, 1 for one arg, 2 for two args (special case for --denoise)
	std::map<std::string, std::pair<std::function<void(int &, int, char *[])>, int>> handlers = {
		{"--folder", {{[&](int &i, int argc, char *argv[])
					   {
						   if (i + 1 >= argc)
							   print_usage_error("--folder", "Missing folder path", argv[0]);
						   settings.folder = argv[++i];
						   if (settings.folder.empty())
							   print_usage_error("--folder", "Folder path cannot be empty", argv[0]);
					   }},
					  1}},
		{"--crop", {{[&](int &i, int argc, char *argv[])
					 {
						 if (i + 1 >= argc)
							 print_usage_error("--crop", "Missing pixel count", argv[0]);
						 try
						 {
							 settings.crop_pixels = std::stoi(argv[++i]);
							 if (settings.crop_pixels < 0)
								 print_usage_error("--crop", "Pixel count must be non-negative", argv[0]);
							 settings.do_crop = true;
						 }
						 catch (const std::exception &e)
						 {
							 print_usage_error("--crop", "Invalid pixel count", argv[0]);
						 }
					 }},
					1}},
		{"--denoise", {{[&](int &i, int argc, char *argv[])
						{
							if (i + 1 >= argc)
								print_usage_error("--denoise", "Missing filter type", argv[0]);
							settings.filter = argv[++i];
							if (settings.filter.empty())
								print_usage_error("--denoise", "Filter type cannot be empty", argv[0]);
							settings.do_denoise = true;
						}},
					   1}}, // 2 args for non-blur, 1 for blur (handled dynamically)
		{"--analyze", {{[&](int &i, int argc, char *argv[])
						{
							if (i + 1 >= argc)
								print_usage_error("--analyze", "Missing output file", argv[0]);
							settings.stats_output = argv[++i];
							if (settings.stats_output.empty())
								print_usage_error("--analyze", "Output file cannot be empty", argv[0]);
							settings.do_analyze = true;
						}},
					   1}},
		{"--calculate-widths", {{[&](int &i, int argc, char *argv[])
								 {
									 if (i + 1 >= argc)
										 print_usage_error("--calculate-widths", "Missing output file", argv[0]);
									 settings.widths_output = argv[++i];
									 if (settings.widths_output.empty())
										 print_usage_error("--calculate-widths", "Output file cannot be empty", argv[0]);
									 settings.do_widths = true;
								 }},
								1}},
		{"--output", {{[&](int &i, int argc, char *argv[])
					   {
						   if (i + 1 >= argc)
							   print_usage_error("--output", "Missing output path", argv[0]);
						   settings.output = argv[++i];
						   if (settings.output.empty())
							   print_usage_error("--output", "Output path cannot be empty", argv[0]);
					   }},
					  1}},
		{"--help", {{[&](int &i, int argc, char *argv[])
					 {
						 std::cout << "Usage: " << argv[0] << " --folder <path> [--crop <pixels>] [--denoise <blur/...> <tile_size>] [--analyze <output.csv>] [--calculate-widths <widths.csv>] [--output <path>]\n";
						 exit(0);
					 }},
					0}}};

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		auto it = handlers.find(arg);
		if (it == handlers.end())
		{
			print_usage_error(arg.c_str(), "Unknown flag", argv[0]);
		}

		auto &[handler, expected_args] = it->second;
		int start_i = i;

		// Execute the handler
		handler(i, argc, argv);

		// Calculate actual args consumed (excluding the flag itself)
		int args_consumed = i - start_i;

		// Validate argument count
		if (args_consumed != expected_args)
		{
			print_usage_error(arg.c_str(), "Incorrect number of arguments", argv[0]);
		}

		// Check that consumed args aren�t flags (except the initial flag)
		for (int j = start_i + 1; j <= i; ++j)
		{
			if (is_flag(argv[j]))
			{
				print_usage_error(arg.c_str(), "Argument cannot be another flag", argv[0]);
			}
		}
	}

	if (settings.folder.empty())
	{
		print_usage_error("--folder", "Required flag missing", argv[0]);
	}

	return settings;
}

namespace cli
{
	void run(int argc, char **argv)
	{
		const char *folder = NULL;
		const char *filter = "none"; // Default preprocessing filter
		char *output = NULL;		 // Default output dir
		const char *stats_output = "results.csv";
		const char *widths_output = "widths.csv";
		int do_crop = 0;
		int do_denoise = 0;
		int do_analyze = 0;
		int do_widths = 0;
		int crop_pixels = 0;
		int denoise_tile_size = 256;

		if (!std::filesystem::exists("assets")) {
			fprintf(stderr, "ERROR: assets folder not found, denoising/deformation analysis will not work properly\n");
		}
		Settings settings = parse_flags(argc, argv);
		folder = settings.folder.c_str();
		do_crop = settings.do_crop;
		crop_pixels = settings.crop_pixels;
		do_denoise = settings.do_denoise;
		filter = settings.filter.c_str();
		do_analyze = settings.do_analyze;
		stats_output = settings.stats_output.c_str();
		do_widths = settings.do_widths;
		widths_output = settings.widths_output.c_str();
		output = settings.output.empty() ? NULL : const_cast<char*>(settings.output.c_str());

		// Validate required input
		if (!folder)
		{
			printf("Error: --folder is required\n");
			printf("Usage: %s --folder <path> [--crop <pixels_from_bottom_to_remove>] [--denoise <blur/sfr_hrsem/sfr_lrsem>] [--analyze <output.csv>] [--output <path_to_folder>]\n", argv[0]);
		}

		// validate filename and load images
		std::vector<uint32_t *> images;
		int width, height;
		bool success = utils::LoadTiffFolder(folder, images, width, height);
		if (!success)
		{
			printf("Failed to load images from %s\n", folder);
			return;
		}

		// validate filter
		const char *models[] = {"blur", "sfr_hrsem", "sfr_hrstem", "sfr_hrtem", "sfr_lrsem", "sfr_lrstem", "sfr_lrtem"};
		if (do_denoise)
		{
			bool found = false;
			for (int i = 0; i < 6; i++)
			{
				if (strcmp(filter, models[i]) == 0)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				printf("Invalid denoise filter: %s\n", filter);
				printf("These are the available filters: blur, sfr_hrsem, sfr_hrstem, sfr_hrtem, sfr_lrsem, sfr_lrstem, sfr_lrtem\n");
				return;
			}
		}

		if (do_crop)
		{
			if (crop_pixels < 0 || crop_pixels > height)
			{
				printf("Invalid crop pixels: %d\n", crop_pixels);
				printf("Crop pixels must be between 0 and %d\n", height);
				return;
			}
			height -= crop_pixels;
		}

		// Execute based on flags
		if (do_denoise)
		{
			if (strcmp(filter, "blur") == 0)
			{
				DenoiseInterface::Blur(images, width, height, 3, 1.0f);
			}
			else
				DenoiseInterface::Denoise(images, width, height, filter, denoise_tile_size, 10);
		}

		if (do_analyze)
		{
			std::vector<std::vector<float>> histograms;
			std::vector<float> avg_histogram;
			std::vector<float> snrs;
			float avg_snr;
			ImageAnalysis::AnalyzeImages(images, width, height, histograms, avg_histogram, snrs, avg_snr);
			utils::saveAnalysisCsv(stats_output, histograms, avg_histogram, snrs, avg_snr);
		}

		if (do_widths)
		{
			auto polygons = CrackDetector::DetectCracks(images, width, height);
			auto widths = FeatureTracker::TrackCrackWidthProfiles(polygons);
			utils::WriteCSV(widths_output, widths);
		}

		if (output != NULL)
		{
			printf("Saving images to %s\n", output);
			if (output[strlen(output) - 1] == '/')
			{
				output[strlen(output) - 1] = '\0';
			}
			if (!std::filesystem::exists(output))
			{
				std::filesystem::create_directory(output);
			}
			for (int i = 0; i < images.size(); i++)
			{
				char filename[256];
				sprintf(filename, "%s/image_%d.tif", output, i);
				utils::WriteTiff(filename, images[i], width, height);
			}
		}

		if (!do_crop && !do_denoise && !do_analyze && !do_widths)
		{
			printf("Nothing to do. Use --crop and/or --denoise and/or --analyze and/or --do-widths.\n");
		}
	}
}
