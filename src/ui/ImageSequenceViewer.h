#pragma once

#include <OpenGL/Texture.h>

#include <string>
#include <vector>
#include <chrono>

class ImageSequenceViewer {
public:
	ImageSequenceViewer();

	void Display();
private:
	void LoadImages(const std::string& path); 
	std::vector<Texture*> m_textures;
	int m_currentFrame = 0;
	bool m_playing = false;
	std::chrono::time_point<std::chrono::steady_clock> m_lasttime;
};
