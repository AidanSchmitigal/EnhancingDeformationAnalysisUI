#include <ui/ImageSequenceViewer.h>
#include <utils.h>

#include <imgui.h>

#include <cstdint>
#include <filesystem>

ImageSequenceViewer::ImageSequenceViewer() {}

void ImageSequenceViewer::Display() {
	// use imgui to display the images
	ImGui::Begin("Image Sequence Viewer");

	if (ImGui::Button("Load Images")) {
		// load the images
		for (int i = 0; i < m_textures.size(); i++) {
			delete m_textures[i];
		}
		m_textures.clear();
		std::string path = utils::OpenFileDialog(".", false);
		LoadImages(path);
	}

	if (ImGui::Button("Play")) {
		m_playing = !m_playing;
	}

	if (m_playing) {
		const auto now = std::chrono::steady_clock::now();
		auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lasttime).count();
		if (diff > 500) {
			m_lasttime = now;
			m_currentFrame++;
			if (m_currentFrame >= m_textures.size()) {
				m_currentFrame = 0;
			}
		}
	}

	if (m_textures.size() == 0) {
		ImGui::Text("No images loaded");
		ImGui::End();
		return;
	}
	else
		ImGui::Image((intptr_t)m_textures[m_currentFrame]->GetID(), ImVec2(m_textures[m_currentFrame]->GetWidth(), m_textures[m_currentFrame]->GetHeight()));

	ImGui::End();
}

void ImageSequenceViewer::LoadImages(const std::string& path) {
	if (!std::filesystem::exists(path)) {
		printf("Path does not exist\n");
		return;
	}
	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		if (entry.path().string().find(".tif") == std::string::npos)
			continue;
		Texture* t = new Texture();
		t->Load(entry.path().c_str());
		m_textures.push_back(t);
	}
}
