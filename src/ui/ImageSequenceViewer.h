#pragma once

#include <OpenGL/Texture.h>

#include <vector>
#include <chrono>

class ImageSequenceViewer {
public:
	ImageSequenceViewer() = default;
	ImageSequenceViewer(std::vector<Texture*>& textures, const std::string& image_sequence_name = "Image Sequence");
	// don't let this free the textures, they are allocated and freed in ImageSet
	~ImageSequenceViewer() {}

	void Display();

	void StartStopPlay() { m_playing = !m_playing; }
	bool GetPlaying() const { return m_playing; }
	void SetCurrentFrame(int frame) { m_currentFrame = frame; }
	void SetPlaySpeed(int speed) { m_playSpeed = speed; }
	void SetTextures(std::vector<Texture*>& textures) { m_textures = textures; m_currentFrame = 0; }

private:
	std::vector<Texture*> m_textures;
	std::string m_imageSequenceName;
	int m_currentFrame = 0;
	bool m_playing = false;
	int m_playSpeed = 1;
	std::chrono::time_point<std::chrono::steady_clock> m_lasttime;
	int m_instanceId = s_instanceCount++;
	static int s_instanceCount;
};
