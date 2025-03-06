#include <ui/PreprocessingTab.h>

#include <utils.h>

#include <core/stabilizer.hpp>
#include <core/CrackDetector.hpp>
#include <core/DenoiseInterface.hpp>

#include <imgui.h>

#include <algorithm>

PreprocessingTab::PreprocessingTab(std::vector<Texture *> & textures, std::vector<Texture *> & processed_textures) {
	m_textures = textures;
	m_processed_textures = processed_textures;
}

void PreprocessingTab::DisplayPreprocessingTab(bool& changed) {
	if (ImGui::BeginTabItem("Preprocessing")) {
		if (m_processed_textures.size() == 0) {
				ImGui::Text("No images loaded");
				ImGui::EndTabItem();
				return;
		}
		ImGui::BeginChild("Controls", ImVec2(250, 0), true);

		// Crop
		ImGui::SeparatorText("Crop");
		static int crop = 60;
		ImGui::SliderInt("Pixels", &crop, 0, 100);
		if (ImGui::Button("Crop Bottom")) {
			if (crop == 0 || crop >= m_processed_textures[0]->GetHeight()) return;
			for (int i = 0; i < m_processed_textures.size(); i++) {
				uint32_t* data = (uint32_t*)malloc(m_processed_textures[i]->GetWidth() * m_processed_textures[i]->GetHeight() * 4);
				m_processed_textures[i]->GetData(data);
				m_processed_textures[i]->Load(data, m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight() - crop);
				free(data);
			}
		}
		ImGui::SameLine(); ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Crops bottom of image (default: 60px for SEM infobar).");

		// Stabilization
		ImGui::SeparatorText("Stabilization");
		if (ImGui::Button("Stabilize")) {
			std::vector<uint32_t*> frames;
			utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
			Stabilizer::Stabilize(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
			utils::LoadDataIntoTexturesAndFree(m_processed_textures, frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}

		// Frame Selection
		ImGui::SeparatorText("Frame Selection");
		if (ImGui::Button("Choose Frames")) {
			ImGui::OpenPopup("Choose Frames");
		}

		if (ImGui::BeginPopup("Choose Frames"))
        {
			if (ImGui::Button("Select All")) {
				for (int i = 0; i < m_processed_textures.size(); i++) m_selected_textures_map[i] = 1;
			}
			ImGui::SameLine();
			if (ImGui::Button("Deselect All")) m_selected_textures_map.clear();
			if (ImGui::Button("Remove Selected")) {
				changed = true;
				std::vector<int> to_remove;
				for (auto& item : m_selected_textures_map) to_remove.push_back(item.first);
				std::sort(to_remove.begin(), to_remove.end());
				for (int i = to_remove.size() - 1; i >= 0; i--) {
					delete m_processed_textures[to_remove[i]];
					m_processed_textures.erase(m_processed_textures.begin() + to_remove[i]);
				}
				m_selected_textures_map.clear();
			}
            ImGui::SeparatorText("Frames");
            for (int i = 0; i < m_processed_textures.size(); i++) {
				char name[100];
				sprintf(name, "Image %d", i);
				bool selected = m_selected_textures_map.find(i) != m_selected_textures_map.end();
				if (selected) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 0.2, 0.2, 1));
				}
				ImGui::ImageButton(name, (ImTextureID)m_processed_textures[i]->GetID(), ImVec2(100, 100));
				if (selected) {
					ImGui::PopStyleColor();
				}
				if (i % 6 != 5) ImGui::SameLine();
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Frame %d", i);
				if (ImGui::IsItemClicked()) {
					if (selected) m_selected_textures_map.erase(i);
					else m_selected_textures_map[i] = 1;
				}
			}
            ImGui::EndPopup();
        }

		// Denoising
		ImGui::SeparatorText("Denoising");
		static int m_kernel_size = 3;
		static float m_sigma = 1.0f;
		ImGui::SetNextItemWidth(150);
		ImGui::SliderInt("Kernel Size", &m_kernel_size, 1, 9, "%.0d");
		if (m_kernel_size % 2 == 0) m_kernel_size++;
		ImGui::SliderFloat("Sigma", &m_sigma, 0.0f, 10.0f);
		if (ImGui::Button("Blur")) {
			std::vector<uint32_t*> frames;
			utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
			DenoiseInterface::Blur(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_kernel_size, m_sigma);
			utils::LoadDataIntoTexturesAndFree(m_processed_textures, frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}
		static int tile_size = 256, overlap = 0, selected_model = 0;
		static const char* models[] = {"sfr_hrsem", "sfr_hrstem", "sfr_hrtem", "sfr_lrsem", "sfr_lrstem", "sfr_lrtem"};
		ImGui::Combo("Model", &selected_model, models, IM_ARRAYSIZE(models));
		ImGui::SliderInt("Tile Size", &tile_size, 32, 512);
		ImGui::SliderInt("Overlap", &overlap, 0, 256);
		static bool result = true;
		if (ImGui::Button("Use AI Model to Denoise")) {
			std::vector<uint32_t*> frames;
			utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
			result = DenoiseInterface::DenoiseNew(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), models[selected_model], tile_size, overlap);
			utils::LoadDataIntoTexturesAndFree(m_processed_textures, frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}
		ImGui::SameLine(); ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Uses Nvidia GPU if available; otherwise slow.");
		if (!result) ImGui::TextColored(ImVec4(1, 0, 0, 1), "Model error!");

		// Crack Detection
		ImGui::SeparatorText("Crack Detection");
		if (ImGui::Button("Detect Cracks")) {
			std::vector<uint32_t*> frames;
			utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
			CrackDetector::DetectCracks(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
			utils::LoadDataIntoTexturesAndFree(m_processed_textures, frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}
		ImGui::SameLine(); ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Early version; works best with high-contrast, denoised images.");

		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::BeginChild("ImageView", ImVec2(0, 0), true);
		if (!m_processed_textures.empty())
			ImGui::Image((ImTextureID)m_processed_textures[0]->GetID(), ImVec2(m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight()));
		ImGui::EndChild();

		ImGui::EndTabItem();
	}
}
