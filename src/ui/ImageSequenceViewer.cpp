#include <ui/ImageSequenceViewer.h>
#include <utils.h>

#include <imgui.h>

#include <cstdint>

int ImageSequenceViewer::s_instanceCount = 0;

ImageSequenceViewer::ImageSequenceViewer(std::vector<Texture*>& textures) : m_textures(textures) {}

void ImageSequenceViewer::Display() {
	ImGui::BeginGroup();

	if (m_textures.size() == 0) {
		ImGui::Text("No images loaded");
	}
	else {
		ImGui::PushID(m_instanceId);
		ImVec2 size = ImGui::GetIO().DisplaySize;
		ImGui::Image((intptr_t)m_textures[m_currentFrame]->GetID(), ImVec2(size.x / 2.1, size.y / 1.5));
		ImGui::SetNextItemWidth(size.x / 2.2);
		ImGui::SliderInt("Frame", &m_currentFrame, 0, m_textures.size() - 1);
		ImGui::PopID();
	}
	ImGui::PushID(m_instanceId);
	if (!m_playing && ImGui::Button("Play")) {
		m_playing = !m_playing;
	}
	if (m_playing && ImGui::Button("Stop")) {
		m_playing = !m_playing;
	}
	ImGui::PopID();
	ImGui::EndGroup();

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
}
