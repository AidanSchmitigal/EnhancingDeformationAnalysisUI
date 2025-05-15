#pragma once

#include <string>
#include <vector>

#include <ui/PreprocessingTab.h>

#include <OpenGL/Texture.h>

#include <core/FeatureTracker.hpp>

class ImageSet {
      public:
	ImageSet(const std::string_view &folder_path);
	~ImageSet();

	void Display();

	bool Closed() { return !m_open; }

      private:
	void LoadImages();
	void DisplayImageComparisonTab();
	void DisplayImageAnalysisTab();
	void DisplayFeatureTrackingTab();
	void DisplayDeformationAnalysisTab();
	void DisplayTestTab();

	static int m_id_counter;

	bool m_open = true;
	std::string m_window_name;
	int m_window_id = 0;
	std::string m_folder_path;
	std::vector<std::shared_ptr<Texture>> m_textures;
	std::vector<std::shared_ptr<Texture>> m_processed_textures;
	uint32_t m_current_frame = 0;

	PreprocessingTab m_preprocessing_tab;

	// feature tracking
	std::vector<cv::Point2f> m_points;
	std::vector<cv::Point2f> m_last_points;
	std::vector<std::vector<cv::Point2f>> m_last_tracked_points;
	uint32_t *m_point_image = nullptr;
	Texture m_point_texture;

	// image analysis
	uint32_t *m_ref_image = nullptr;
	uint32_t m_ref_image_width = 0, m_ref_image_height = 0;
	std::vector<std::vector<float>> histograms;
	std::vector<float> avg_histogram;
	std::vector<float> snrs;
	float avg_snr = 0.0f;
	bool write_success = true;

	// feature tracking
	std::vector<std::vector<std::vector<float>>> m_widths;
	std::vector<std::vector<float>> m_manual_widths;
	bool m_widths_write_success = true;
	std::chrono::time_point<std::chrono::system_clock> m_last_time =
	    std::chrono::system_clock::now();
};
