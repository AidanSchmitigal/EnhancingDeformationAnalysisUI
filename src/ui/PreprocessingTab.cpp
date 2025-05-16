#include <ui/PreprocessingTab.h>

#include <utils.h>

#include <core/CrackDetector.hpp>
#include <core/DenoiseInterface.hpp>
#include <core/Stabilizer.hpp>
#include <core/ThreadPool.hpp>

#include <imgui.h>

#include <algorithm>

// Define static model names
const char *PreprocessingTab::m_models[] = {"sfr_hrsem", "sfr_hrstem", "sfr_hrtem",
					    "sfr_lrsem", "sfr_lrstem", "sfr_lrtem"};

PreprocessingTab::PreprocessingTab(std::vector<std::shared_ptr<Texture>> &textures,
				   std::vector<std::shared_ptr<Texture>> &processed_textures) {
	m_textures = textures;
	m_processed_textures = processed_textures;
}

void PreprocessingTab::OnProcessingComplete(bool success) {
	m_last_result = success;

	if (success && !m_processing_frames.empty()) {
		// Load the processed data back into textures
		utils::LoadDataIntoTexturesAndFree(m_processed_textures, m_processing_frames,
						   m_processed_textures[0]->GetWidth(),
						   m_processed_textures[0]->GetHeight());
	} else {
		// Free any allocated memory in case of failure
		for (auto frame : m_processing_frames) {
			free(frame);
		}
	}

	m_processing_frames.clear();
	m_is_processing = false;
}

void PreprocessingTab::DisplayPreprocessingTab(bool &changed) {
	if (ImGui::BeginTabItem("Preprocessing")) {
		if (m_processed_textures.size() == 0) {
			ImGui::Text("No images loaded");
			ImGui::EndTabItem();
			return;
		}

		// Check if any async processing has completed
		if (m_is_processing && m_processing_future && m_processing_future->valid()) {
			// Poll the future with zero timeout to check if it's
			// done without blocking
			auto status = m_processing_future->wait_for(std::chrono::seconds(0));
			if (status == std::future_status::ready) {
				// Get the result and update the UI
				bool result = m_processing_future->get();
				OnProcessingComplete(result);
			}
		}

		ImGui::BeginChild("Controls", ImVec2(250, 0), true);

		// Processing status display
		if (m_is_processing) {
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Processing...");
			float progress = 0.0f;
			if (DenoiseInterface::IsProcessing()) {
				progress = DenoiseInterface::GetProgress();
			} else if (Stabilizer::IsProcessing()) {
				progress = Stabilizer::GetProgress();
			} else if (CrackDetector::IsProcessing()) {
				progress = CrackDetector::GetProgress();
			}
			ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
			// Disable other buttons during processing
		}

		// Crop
		ImGui::SeparatorText("Crop");
		static int crop = 60;
		ImGui::BeginDisabled(m_is_processing);
		ImGui::SliderInt("Pixels", &crop, 0, 100);
		if (ImGui::Button("Crop Bottom") && !m_is_processing) {
			if (crop == 0 || crop >= m_processed_textures[0]->GetHeight())
				return;
			for (int i = 0; i < m_processed_textures.size(); i++) {
				uint32_t *data = (uint32_t *)malloc(m_processed_textures[i]->GetWidth() *
								    m_processed_textures[i]->GetHeight() * 4);
				m_processed_textures[i]->GetData(data);
				m_processed_textures[i]->Load(data, m_processed_textures[i]->GetWidth(),
							      m_processed_textures[i]->GetHeight() - crop);
				free(data);
			}
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Crops bottom of image (default: "
					  "60px for SEM infobar).");

		// Stabilization
		ImGui::SeparatorText("Stabilization");
		ImGui::BeginDisabled(m_is_processing);
		if (ImGui::Button("Stabilize")) {
			m_is_processing = true;

			// Copy image data
			m_processing_frames.clear();
			utils::GetDataFromTextures(m_processing_frames, m_processed_textures[0]->GetWidth(),
						   m_processed_textures[0]->GetHeight(), m_processed_textures);

			// Process asynchronously
			auto width = m_processed_textures[0]->GetWidth();
			auto height = m_processed_textures[0]->GetHeight();

			auto future =
			    Stabilizer::StabilizeAsync(m_processing_frames, width, height, [this](bool result) {
				    // This callback will run in the worker
				    // thread We don't need to do anything here
				    // as we check the future in the main loop
			    });
			m_processing_future = std::make_shared<std::future<bool>>(std::move(future));
		}
		ImGui::EndDisabled();

		// Frame Selection
		ImGui::SeparatorText("Frame Selection");
		ImGui::BeginDisabled(m_is_processing);

		static bool choose_frames_open = false;
		if (ImGui::Button("Choose Frames")) {
			choose_frames_open = true;
		}

		// Create a callback for removing selected frames
		auto removeSelectedFrames = [this, &changed]() {
			changed = true;
			std::vector<int> to_remove;
			for (auto &item : m_selected_textures_map)
				to_remove.push_back(item.first);
			std::sort(to_remove.begin(), to_remove.end());
			for (int i = to_remove.size() - 1; i >= 0; i--) {
				m_processed_textures.erase(m_processed_textures.begin() + to_remove[i]);
			}
			m_selected_textures_map.clear();
		};

		// Use the common UI function to display the frame selection window
		ui::DisplayFrameSelectionWindow("Frame Selection", choose_frames_open, m_processed_textures,
						m_selected_textures_map, removeSelectedFrames);
		ImGui::EndDisabled();

		// Denoising
		ImGui::SeparatorText("Denoising");
		ImGui::BeginDisabled(m_is_processing);
		ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Kernel Size").x);
		ImGui::SliderInt("Kernel Size", &m_kernel_size, 1, 9, "%.0d");
		if (m_kernel_size % 2 == 0)
			m_kernel_size++;
		ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Sigma").x);
		ImGui::SliderFloat("Sigma", &m_sigma, 0.0f, 10.0f);
		if (ImGui::Button("Blur")) {
			m_is_processing = true;

			// Copy image data
			m_processing_frames.clear();
			utils::GetDataFromTextures(m_processing_frames, m_processed_textures[0]->GetWidth(),
						   m_processed_textures[0]->GetHeight(), m_processed_textures);

			// Process asynchronously
			auto width = m_processed_textures[0]->GetWidth();
			auto height = m_processed_textures[0]->GetHeight();
			auto kernel_size = m_kernel_size;
			auto sigma = m_sigma;

			// Use the async version with callback
			// Honestly it runs quick enough this shouldn't matter
			// but for the sake of consistency we can use the async
			// version
			auto future = DenoiseInterface::BlurAsync(m_processing_frames, width, height, kernel_size,
								  sigma, [this](bool result) {
									  // This callback will run in the worker
									  // thread We don't need to do anything here
									  // as we check the future in the main loop
								  });

			m_processing_future = std::make_shared<std::future<bool>>(std::move(future));
		}

#ifdef UI_INCLUDE_TENSORFLOW
		utils::CreateTileTextures(m_split_textures, m_processed_textures[0], m_tile_config);

		ImGui::SeparatorText("AI Denoising");
		ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Model").x);
		ImGui::Combo("Model", &m_selected_model, m_models, IM_ARRAYSIZE(m_models));
		ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Tiling Type").x);
		ImGui::Combo("Tiling Type", (int *)&m_tile_config.type, "Cropped\0Blended\0\0");
		ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Tile Size").x);
		ImGui::SliderInt("Tile Size", &m_tile_config.tileSize, 150, 512);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("We recommend leaving this at 256. Smaller or larger numbers can cause the "
					  "models to emit errors, but we leave the choice to you.");
		if (m_tile_config.type == TileType::Blended) {
			ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Overlap").x);
			ImGui::SliderInt("Overlap", &m_tile_config.overlap, 0, 128);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Overlap size. MUST BE LESS THAN TILE "
						  "SIZE!\nGenerally "
						  "leave around 0, but can be varied for "
						  "different results.");
		} else if (m_tile_config.type == TileType::Cropped) {
			ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Center Size").x);
			ImGui::SliderInt("Center Size", &m_tile_config.centerSize, 0, 512);
			ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Include Outside").x);
			ImGui::Checkbox("Include Outside", &m_tile_config.includeOutside);
		}

		static bool show_tiled_image_open = false;
		if (ImGui::Button("Show One Tiled Image")) {
			show_tiled_image_open = true;
			// Ensure we have tiles to display when opening the window
			if (m_split_textures.empty() && !m_processed_textures.empty()) {
				utils::CreateTileTextures(m_split_textures, m_processed_textures[0], m_tile_config);
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Click to show the tiled image.\nThis will split "
					  "the first image in the sequence into tiles and "
					  "show them in a window.\nNote: This is not the "
					  "final result, just a preview of the tiles.");

		// Create a refresh callback
		auto refreshTiles = [this]() {
			m_split_textures.clear();
			if (!m_processed_textures.empty()) {
				utils::CreateTileTextures(m_split_textures, m_processed_textures[0], m_tile_config);
			}
		};

		// Use the common UI function to display the tile preview window
		ui::DisplayTilePreviewWindow("Tiled Image Preview", show_tiled_image_open, m_split_textures,
					     refreshTiles);

		if (ImGui::Button("Denoise")) {
			m_is_processing = true;

			// Copy image data
			m_processing_frames.clear();
			utils::GetDataFromTextures(m_processing_frames, m_processed_textures[0]->GetWidth(),
						   m_processed_textures[0]->GetHeight(), m_processed_textures);

			// Process asynchronously
			auto width = m_processed_textures[0]->GetWidth();
			auto height = m_processed_textures[0]->GetHeight();
			auto model_name = std::string(m_models[m_selected_model]);
			auto tile_size = m_tile_size;
			auto center_size = m_center_size;
			auto include_outside = m_include_outside;

			// Use the async version
			auto future = DenoiseInterface::DenoiseAsync(m_processing_frames, width, height, model_name,
								     m_tile_config, [this](bool result) {
									     // This callback will run in the worker
									     // thread We don't need to do anything here
									     // as we check the future in the main loop
								     });

			m_processing_future = std::make_shared<std::future<bool>>(std::move(future));
		}
#endif
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Uses Nvidia GPU if available; otherwise slow.");
		if (!m_last_result)
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "Model error!");

		// Crack Detection
		ImGui::SeparatorText("Crack Detection");
		ImGui::BeginDisabled(m_is_processing);
		ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Crack Darkness").x);
		ImGui::SliderInt("Crack Darkness", &m_crack_darkness, 0, 127);
		ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Fill Threshold").x);
		ImGui::SliderInt("Fill Threshold", &m_fill_threshold, 0, 127);
		ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Sharpness").x);
		ImGui::SliderInt("Sharpness", &m_sharpness, 0, 127);
		ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Resolution").x);
		ImGui::SliderInt("Resolution", &m_resolution, 0, 50);
		ImGui::SetNextItemWidth(235 - ImGui::CalcTextSize("Amount").x);
		ImGui::SliderInt("Amount", &m_amount, 0, 20);
		if (ImGui::Button("Detect Cracks")) {
			m_is_processing = true;

			// Copy image data
			m_processing_frames.clear();
			utils::GetDataFromTextures(m_processing_frames, m_processed_textures[0]->GetWidth(),
						   m_processed_textures[0]->GetHeight(), m_processed_textures);

			// Process asynchronously
			auto width = m_processed_textures[0]->GetWidth();
			auto height = m_processed_textures[0]->GetHeight();

			auto future = CrackDetector::DetectCracksAsync(m_processing_frames, width, height,
								       m_crack_darkness, // crack_darkness
								       m_fill_threshold, // fill_threshold
								       m_sharpness,	 // sharpness
								       m_resolution,	 // resolution
								       m_amount,	 // amount
								       [this](bool result) {
									       // This callback will run in the worker
									       // thread We don't need to do anything
									       // here as we check the future in the
									       // main loop
								       });

			m_processing_future = std::make_shared<std::future<bool>>(std::move(future));
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("This will highlight in the image what it thinks "
					  "is the border "
					  "of the cracks.\nIt is recommended to crop the "
					  "infobar before "
					  "using this, as it is detected as a crack.");

		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::BeginChild("ImageView", ImVec2(0, 0), true);
		if (!m_processed_textures.empty())
			ImGui::Image((ImTextureID)m_processed_textures[0]->GetID(),
				     ImVec2(m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight()));
		ImGui::EndChild();

		ImGui::EndTabItem();
	}
}
