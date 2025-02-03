#include <ui/ImageSet.h>

#include <ui/ImageSequenceViewer.h>
#include <core/stabilizer.hpp>
#include <core/CrackDetector.hpp>
#include <utils.h>

#include <imgui.h>

#include <string>
#include <filesystem>
#include <unordered_map>

int ImageSet::m_id_counter = 0;

int playspeed = 1;
std::unordered_map<int, int> selected_textures_map;

ImageSet::ImageSet(const std::string_view &folder_path) : m_folder_path(folder_path) {
	m_window_name = "ImageSet " + std::to_string(m_id_counter++);

	LoadImages();

	m_sequence_viewer = ImageSequenceViewer(m_textures, "Original Images");
	m_processed_sequence_viewer = ImageSequenceViewer(m_processed_textures, "Processed Images");
}

void ImageSet::Display() {
	ImGui::Begin(m_window_name.c_str());
	ImGui::BeginTabBar("PreProcessing");

	if (ImGui::BeginTabItem("Image Comparison")) {
		ImGui::NewLine();
		if (ImGui::Button("Play Both")) {
			m_sequence_viewer.StartStopPlay();
			m_processed_sequence_viewer.StartStopPlay();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Processed Images")) {
			if (m_textures.size() != m_processed_textures.size()) {
				for (int i = m_processed_textures.size(); i < m_textures.size(); i++) {
					m_processed_textures.push_back(new Texture());
				}
			}
			for (int i = 0; i < m_textures.size(); i++) {
				uint32_t* data = (uint32_t*)malloc(m_textures[i]->GetWidth() * m_textures[i]->GetHeight() * 4);
				m_textures[i]->GetData(data);
				m_processed_textures[i]->Load(data, m_textures[i]->GetWidth(), m_textures[i]->GetHeight());
				free(data);
			}
			m_processed_sequence_viewer.SetTextures(m_processed_textures);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetIO().DisplaySize.x / 2.2);
		ImGui::SliderInt("Speed", &playspeed, 1, 10);
		m_sequence_viewer.SetPlaySpeed(playspeed);
		m_processed_sequence_viewer.SetPlaySpeed(playspeed);
		m_sequence_viewer.Display();
		ImGui::SameLine();
		m_processed_sequence_viewer.Display();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Preprocessing")) {
		if (ImGui::TreeNode("Crop")) {
			if (ImGui::Button("Crop Bottom of Image")) {
				// crop off 60 pixels from the bottom of the images
				for (int i = 0; i < m_textures.size(); i++) {
					uint32_t* data = (uint32_t*)malloc(m_textures[i]->GetWidth() * (m_textures[i]->GetHeight()) * 4);
					m_textures[i]->GetData(data);
					m_processed_textures[i]->Load(data, m_textures[i]->GetWidth(), m_textures[i]->GetHeight() - 60);
					free(data);
				}
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Stabilization")) {
			if (ImGui::Button("Stabilize")) {
				for (auto& item : selected_textures_map) {
					int i = item.first;
					m_processed_textures.erase(m_processed_textures.begin() + i);
				}
				std::vector<uint32_t*> frames;
				for (int i = 0; i < m_processed_textures.size(); i++) {
					uint32_t* data = (uint32_t*)malloc(m_processed_textures[i]->GetWidth() * m_processed_textures[i]->GetHeight() * 4);
					m_processed_textures[i]->GetData(data);
					frames.push_back(data);
				}
				Stabilizer::stabilize(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
				for (int i = 0; i < frames.size(); i++) {
					m_processed_textures[i]->Load(frames[i], m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight());
					free(frames[i]);
				}
				selected_textures_map.clear();
				m_processed_sequence_viewer.SetTextures(m_processed_textures);
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove Selected")) {
				std::vector<int> to_remove;
				for (auto& item : selected_textures_map) {
					to_remove.push_back(item.first);
				}
				std::sort(to_remove.begin(), to_remove.end());
				for (int i = to_remove.size() - 1; i >= 0; i--) {
						m_processed_textures.erase(m_processed_textures.begin() + to_remove[i]);
				}
				selected_textures_map.clear();
				m_processed_sequence_viewer.SetTextures(m_processed_textures);
			}
			for (int i = 0; i < m_processed_textures.size(); i++) {
				char name[100];
				sprintf(name, "Frame %d", i);
				bool selected = selected_textures_map.find(i) != selected_textures_map.end();
				if (selected) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(200, 0, 0, 255));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(255, 0, 0, 255));
				}
				if (ImGui::ImageButton(name, (ImTextureID)m_processed_textures[i]->GetID(), ImVec2(100, 100))) {
					if (selected_textures_map.find(i) == selected_textures_map.end()) {
						selected_textures_map[i] = 1;
					}
					else {
						selected_textures_map.erase(i);
					}
				}
				if (selected) {
					ImGui::PopStyleColor(2);
				}
				if (i % 7 != 6) {
					ImGui::SameLine();
				}
			}
			ImGui::TreePop();
		}

		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Crack Detection")) {
			if (ImGui::Button("Detect Cracks")) {
					std::vector<uint32_t*> frames;
					for (int i = 0; i < m_processed_textures.size(); i++) {
							uint32_t* data = (uint32_t*)malloc(m_processed_textures[i]->GetWidth() * m_processed_textures[i]->GetHeight() * 4);
							m_processed_textures[i]->GetData(data);
							frames.push_back(data);
					}
					CrackDetector::detectCracks(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
					for (int i = 0; i < frames.size(); i++) {
							m_processed_textures[i]->Load(frames[i], m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight());
							free(frames[i]);
					}
			}

			ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Deformation Analysis/Prediction")) {
		ImGui::Text("Deformation Analysis tab");
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
	ImGui::End();
}

void ImageSet::LoadImages() {
	if (!std::filesystem::exists(m_folder_path)) {
		printf("Path does not exist\n");
		return;
	}
	std::vector<std::string> files;
	for (const auto& entry : std::filesystem::directory_iterator(m_folder_path)) {
		if (entry.path().string().find(".tif") == std::string::npos)
			continue;
		files.push_back(entry.path().string());
	}
	std::sort(files.begin(), files.end());
	for (const auto& file : files) {
		int width, height;
		uint32_t* temp = utils::LoadTiff(file.c_str(), width, height);
		Texture* t = new Texture();
		t->Load(temp, width, height);
		m_textures.push_back(t);
		Texture* t2 = new Texture();
		t2->Load(temp, width, height);
		m_processed_textures.push_back(t2);
		free(temp);
	}
}
