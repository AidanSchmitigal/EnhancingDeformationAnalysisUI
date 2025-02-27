#pragma once

#include <string>
#include <vector>

#include <ui/ImageSequenceViewer.h>
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

	PreprocessingTab m_preprocessing_tab;
	std::vector<cv::Point2f> m_points;
	uint32_t* m_point_image = nullptr;
	Texture m_point_texture;
	bool m_open = true;
	std::string m_window_name;
	std::string m_folder_path;
	std::vector<Texture *> m_textures;
	std::vector<Texture *> m_processed_textures;
	ImageSequenceViewer m_sequence_viewer;
	ImageSequenceViewer m_processed_sequence_viewer;
};
