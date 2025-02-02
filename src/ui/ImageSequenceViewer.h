#pragma once

#include <OpenGL/Texture.h>

#include <vector>
#include <chrono>

class ImageSequenceViewer {
public:
	ImageSequenceViewer() = default;
	ImageSequenceViewer(std::vector<Texture*>& textures);

	void Display();
private:
	std::vector<Texture*> m_textures;
	int m_currentFrame = 0;
	bool m_playing = false;
	std::chrono::time_point<std::chrono::steady_clock> m_lasttime;
	int m_instanceId = s_instanceCount++;
	static int s_instanceCount;
};
