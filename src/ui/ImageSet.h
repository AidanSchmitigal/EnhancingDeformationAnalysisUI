#pragma once

#include <string>
#include <vector>

#include <ui/ImageSequenceViewer.h>
#include <OpenGL/Texture.h>

class ImageSet {
public:
	ImageSet(const std::string_view &folder_path);

	void Display();

private:
	void LoadImages();
	void DisplayImageComparisonTab();
	void DisplayPreprocessingTab();

	static int m_id_counter;
	std::string m_window_name;
	std::string m_folder_path;
	std::vector<Texture *> m_textures;
	std::vector<Texture *> m_processed_textures;
	ImageSequenceViewer m_sequence_viewer;
	ImageSequenceViewer m_processed_sequence_viewer;
};
