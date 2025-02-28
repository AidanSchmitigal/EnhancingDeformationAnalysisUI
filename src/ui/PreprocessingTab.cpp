#include <ui/PreprocessingTab.h>

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
		if (ImGui::CollapsingHeader("Crop")) {
			static int crop = 60;
			ImGui::SetNextItemWidth(ImGui::GetIO().DisplaySize.x / 2.3);
			ImGui::SliderInt("Pixels to Crop", &crop, 0, 100);
			if (ImGui::Button("Crop Bottom of Image")) {
				// crop off 60 pixels from the bottom of the images
				for (int i = 0; i < m_processed_textures.size(); i++) {
					uint32_t* data = (uint32_t*)malloc(m_processed_textures[i]->GetWidth() * (m_processed_textures[i]->GetHeight()) * 4);
					m_processed_textures[i]->GetData(data);
					m_processed_textures[i]->Load(data, m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight() - crop);
					free(data);
				}
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("This will crop off the bottom of the image by the specified number of pixels.\nThe default is 60 pixels, which is the size of the infobar on many SEM images.");
				ImGui::EndTooltip();
			}
		}
		if (ImGui::CollapsingHeader("Stabilization")) {
			if (ImGui::Button("Stabilize")) {
				std::vector<uint32_t*> frames;
				for (int i = 0; i < m_processed_textures.size(); i++) {
					uint32_t* data = (uint32_t*)malloc(m_processed_textures[i]->GetWidth() * m_processed_textures[i]->GetHeight() * 4);
					m_processed_textures[i]->GetData(data);
					frames.push_back(data);
				}

				Stabilizer::Stabilize(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
				for (int i = 0; i < frames.size(); i++) {
					m_processed_textures[i]->Load(frames[i], m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight());
					free(frames[i]);
				}
			}
		}

		if (ImGui::CollapsingHeader("Frame Selection")) {
			if (ImGui::Button("Select All")) {
				for (int i = 0; i < m_processed_textures.size(); i++) {
					m_selected_textures_map[i] = 1;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Deselect All")) {
				m_selected_textures_map.clear();
			}
			if (ImGui::Button("Remove Selected")) {
				changed = true;
				std::vector<int> to_remove;
				for (auto& item : m_selected_textures_map) {
					to_remove.push_back(item.first);
				}
				std::sort(to_remove.begin(), to_remove.end());
				for (int i = to_remove.size() - 1; i >= 0; i--) {
					delete m_processed_textures[to_remove[i]];
					m_processed_textures.erase(m_processed_textures.begin() + to_remove[i]);
				}
				m_selected_textures_map.clear();
			}
			int images_per_line = ImGui::GetIO().DisplaySize.x / 115;
			for (int i = 0; i < m_processed_textures.size(); i++) {
				char name[100];
				sprintf(name, "Frame %d", i);
				bool selected = m_selected_textures_map.find(i) != m_selected_textures_map.end();
				if (selected) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(200, 0, 0, 255));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(255, 0, 0, 255));
				}
				if (ImGui::ImageButton(name, (ImTextureID)m_processed_textures[i]->GetID(), ImVec2(100, 100))) {
					if (m_selected_textures_map.find(i) == m_selected_textures_map.end()) {
						m_selected_textures_map[i] = 1;
					}
					else {
						m_selected_textures_map.erase(i);
					}
				}
				if (selected) {
					ImGui::PopStyleColor(2);
				}
				if (i % images_per_line != images_per_line-1 && i != m_processed_textures.size() - 1) {
					ImGui::SameLine();
				}
			}
		}

		if (ImGui::CollapsingHeader("Denoising")) {
			static int m_kernel_size = 3;
			static float m_sigma = 1.0f;
			ImGui::Text("Gaussian Blur");
			ImGui::SetNextItemWidth(ImGui::GetIO().DisplaySize.x / 2.3);
			if (ImGui::SliderInt("Kernel Size", &m_kernel_size, 1, 9, "%.0d")) {
				if (m_kernel_size % 2 == 0) {
					m_kernel_size++;
				}
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetIO().DisplaySize.x / 2.3);
			ImGui::SliderFloat("Sigma", &m_sigma, 0.0f, 10.0f);
			if (ImGui::Button("Blur")) {
				std::vector<uint32_t*> frames;
				for (int i = 0; i < m_processed_textures.size(); i++) {
					uint32_t* data = (uint32_t*)malloc(m_processed_textures[i]->GetWidth() * m_processed_textures[i]->GetHeight() * 4);
					m_processed_textures[i]->GetData(data);
					frames.push_back(data);
				}
				DenoiseInterface::Blur(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_kernel_size, m_sigma);
				for (int i = 0; i < frames.size(); i++) {
					m_processed_textures[i]->Load(frames[i], m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight());
					free(frames[i]);
				}
			}

			ImGui::NewLine();

			static int tile_size = 256;
			static int overlap = 0;
			ImGui::Text("Denoising using tk_r_em AI models");
			static int selected_model = 0;
			const char* models[] = { "sfr_hrsem", "sfr_hrstem", "sfr_hrtem", "sfr_lrsem", "sfr_lrstem", "sfr_lrtem" };
			ImGui::SetNextItemWidth(ImGui::GetIO().DisplaySize.x / 3.5);
			ImGui::Combo("Model", &selected_model, models, IM_ARRAYSIZE(models));
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetIO().DisplaySize.x / 3.5);
			ImGui::SliderInt("Tile Size", &tile_size, 32, 512);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetIO().DisplaySize.x / 3.5);
			ImGui::SliderInt("Overlap", &overlap, 0, 256);
			static bool result = true;

			if (ImGui::Button("Denoise")) {
				std::vector<uint32_t*> frames;
				for (int i = 0; i < m_processed_textures.size(); i++) {
					uint32_t* data = (uint32_t*)malloc(m_processed_textures[i]->GetWidth() * m_processed_textures[i]->GetHeight() * 4);
					m_processed_textures[i]->GetData(data);
					frames.push_back(data);
				}

				result = DenoiseInterface::DenoiseNew(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), models[selected_model], tile_size, overlap);
				for (int i = 0; i < frames.size(); i++) {
					m_processed_textures[i]->Load(frames[i], m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight());
					free(frames[i]);
				}

			}
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("Denoising using tk_r_em will attempt to use an Nvidia GPU if you have one with cuda and cuDNN installed.\nIf you do not have an Nvidia GPU, it will take a VERY long time.");
				ImGui::EndTooltip();
			}
			if (!result) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
				ImGui::Text("Model exited with an error!");
				ImGui::PopStyleColor();
			}
		}

		if (ImGui::CollapsingHeader("Crack Detection")) {
			if (ImGui::Button("Detect Cracks")) {
				std::vector<uint32_t*> frames;
				for (int i = 0; i < m_processed_textures.size(); i++) {
					uint32_t* data = (uint32_t*)malloc(m_processed_textures[i]->GetWidth() * m_processed_textures[i]->GetHeight() * 4);
					m_processed_textures[i]->GetData(data);
					frames.push_back(data);
				}

				CrackDetector::DetectCracks(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
				for (int i = 0; i < frames.size(); i++) {
					m_processed_textures[i]->Load(frames[i], m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight());
					free(frames[i]);
				}
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("This is an early version of the crack detection algorithm. It works best on images with high contrast between the cracks and the background. \n\n"
						"To use it, first denoise the images using the \"Denoising\" section. Then, click the \"Detect Cracks\" button. The algorithm will then process the images and display the results.");
				ImGui::EndTooltip();
			}
		}
		ImGui::EndTabItem();
	}
}