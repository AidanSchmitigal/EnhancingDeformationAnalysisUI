#include <ui/ImageSet.h>

#include <core/CrackDetector.hpp>
#include <core/DenoiseInterface.hpp>
#include <core/FeatureTracker.hpp>
#include <core/DeformationAnalysisInterface.hpp>
#include <core/ImageAnalysis.hpp>

#include <utils.h>

#include <imgui.h>

#include <opencv2/opencv.hpp>

#include <string>

int ImageSet::m_id_counter = 0;

ImageSet::ImageSet(const std::string_view &folder_path) : m_folder_path(folder_path) {
	m_window_name = folder_path.find_last_of('/') == std::string::npos ? folder_path : folder_path.substr(folder_path.find_last_of('/') + 1);
	m_window_id = m_id_counter++;

	LoadImages();

	m_point_texture = Texture();

	m_preprocessing_tab = PreprocessingTab(m_textures, m_processed_textures);
}

ImageSet::~ImageSet() {
	free(m_point_image);
}

// display the image set window and the tabs
void ImageSet::Display() {
	ImGui::Begin((m_window_name + " " + std::to_string(m_window_id)).c_str(), &m_open);

	// Check if processing is happening in the preprocessing tab
	bool isPreprocessProcessing = m_preprocessing_tab.IsProcessing();
	bool isDeformationProcessing = DeformationAnalysisInterface::IsProcessing();

	// Start tab bar
	ImGui::BeginTabBar("Image Set Tabs");

	bool changed = false;

	// If processing, only allow the Preprocessing tab
	if (!isPreprocessProcessing && !isDeformationProcessing) {
		DisplayImageComparisonTab();
	}

	if (isPreprocessProcessing || isDeformationProcessing) {
		ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.8f, 0.5f, 0.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.9f, 0.6f, 0.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(1.0f, 0.7f, 0.0f, 1.0f));
	}

	// if we aren't doing deformation analysis, show the preprocessing tab
	if (!isDeformationProcessing)
		m_preprocessing_tab.DisplayPreprocessingTab(changed);

	if (isPreprocessProcessing || isDeformationProcessing) {
		ImGui::PopStyleColor(3);
	}

	// if we aren't doing preprocessing, show the deformation analysis tab
	if (!isPreprocessProcessing)
		DisplayDeformationAnalysisTab();

	// Display the other tabs only if not processing
	if (!isPreprocessProcessing) {
		DisplayFeatureTrackingTab();
		DisplayImageAnalysisTab();
	}

	// only necessary when we modify the vector by changing its size (this only happens in the preprocessing tab)
	if (changed) {
		m_preprocessing_tab.GetProcessedTextures(m_processed_textures);
	}

	ImGui::EndTabBar();
	ImGui::End();
}

void ImageSet::LoadImages() {
	PROFILE_FUNCTION();

	std::vector<uint32_t*> images;
	int width, height;
	utils::LoadTiffFolder(m_folder_path.c_str(), images, width, height);

	for (auto& image : images) {
		std::shared_ptr<Texture> t = std::make_shared<Texture>();
		t->Load(image, width, height);
		m_textures.push_back(t);
		std::shared_ptr<Texture> t2 = std::make_shared<Texture>();
		t2->Load(image, width, height);
		m_processed_textures.push_back(t2);
		free(image);
	}
}

// TODO: change to incorporate the original images and images from preprocessing, feature tracking, and deformation analysis (all separate)
void ImageSet::DisplayImageComparisonTab() {
	static bool isPlaying = false;
	if (ImGui::BeginTabItem("Image Comparison")) {
		ImGui::BeginChild("Controls", ImVec2(250, 0), true);
		{ // put into scope for visibility
			// Control buttons
			if (ImGui::Button("Play/Pause")) isPlaying = !isPlaying;
			if (ImGui::Button("Reset Playback")) {
				m_current_frame = 0;
				isPlaying = false;
			}
			if (ImGui::Button("Reset Processed Images")) {
				PROFILE_SCOPE(ResetProcessedImages);

				while (m_textures.size() > m_processed_textures.size())
					m_processed_textures.push_back(std::make_shared<Texture>());
				ImVec2 size = ImVec2(m_textures[0]->GetWidth(), m_textures[0]->GetHeight());
				uint32_t* data = (uint32_t*)malloc(size.x * size.y * sizeof(uint32_t));
				for (int i = 0; i < m_textures.size(); i++) {
					m_textures[i]->GetData(data);
					m_processed_textures[i]->Load(data, (int)size.x, (int)size.y);
				}
				m_preprocessing_tab.SetProcessedTextures(m_processed_textures);
				free(data);
			}
			ImGui::SameLine();
			ImGui::TextDisabled("?");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("This will reset the processed images back to their original unprocessed state.");

			if (ImGui::Button("Save Processed Images")) {
				std::string folder = utils::OpenFileDialog(".", "Choose a Folder to Save the Images", true);
				if (!folder.empty() && m_processed_textures.size() > 0) {
					uint32_t* data = new uint32_t[m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight()];
					for (int i = 0; i < m_processed_textures.size(); i++) {
						char path[256];
						sprintf(path, "%s/frame_%d.tif", folder.c_str(), i);
						m_processed_textures[i]->GetData(data);
						utils::WriteTiff(path, data, m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight());
					}
					free(data);
				}
			}
			ImGui::SameLine();
			ImGui::TextDisabled("?");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("This will write the processed images into a folder of your choosing.");

			if (ImGui::Button("Save a GIF")) {
				std::string path = utils::SaveFileDialog(".", "Choose Where to Save the GIF", "gif");
				if (!path.empty()) {
					utils::WriteGIFOfImageSet(path.c_str(), m_processed_textures, 40, 0);
				}
			}
			ImGui::SameLine();
			ImGui::TextDisabled("?");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("This will write a GIF of the processed images, and can take some time with larger image sets.");

			// Frame slider
			ImGui::SliderInt("Frame", (int*)&m_current_frame, 0, (int)std::max(m_textures.size(), m_processed_textures.size()) - 1);
		}
		ImGui::EndChild();

		ImGui::SameLine();

		// Side-by-side image display
		ImGui::BeginChild("Images", ImVec2(0, 0), true);
		{ // put into scope again for visibility
			// Left sequence
			ImGui::SeparatorText("Original Sequence");
			if (!m_textures.empty() && m_current_frame < m_textures.size()) {
				ImGui::Image(m_textures[m_current_frame]->GetID(), ImVec2(m_textures[0]->GetWidth() / 1.5, m_textures[0]->GetHeight() / 1.5));
			}

			// Right sequence
			ImGui::SeparatorText("Processed Sequence");
			if (!m_processed_textures.empty() && m_current_frame < m_processed_textures.size()) {
				ImGui::Image(m_processed_textures[m_current_frame]->GetID(), ImVec2((float)m_processed_textures[0]->GetWidth() / 1.5, (float)m_processed_textures[0]->GetHeight() / 1.5));
			}
		}
		ImGui::EndChild();

		// Playback logic
		if (isPlaying) {
			m_current_frame++;
			if (m_current_frame >= std::max(m_textures.size(), m_processed_textures.size())) {
				m_current_frame = 0;  // Loop back to start
			}
		}
		ImGui::EndTabItem();
	}
}

void ImageSet::DisplayImageAnalysisTab() {
	static uint32_t* m_ref_image = nullptr;
	static uint32_t m_ref_image_width = 0, m_ref_image_height = 0;
	static std::vector<std::vector<float>> histograms;
	static std::vector<float> avg_histogram;
	static std::vector<float> snrs;
	static float avg_snr = 0.0f;
	static bool write_success = true;
	if (ImGui::BeginTabItem("Image Analysis")) {
		if (m_processed_textures.size() == 0) {
			ImGui::Text("No images loaded");
			ImGui::EndTabItem();
			return;
		}
		if (m_ref_image == nullptr || m_ref_image_width * m_ref_image_height != m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight()) {
			if (m_ref_image) free(m_ref_image);
			m_ref_image_width = m_processed_textures[0]->GetWidth();
			m_ref_image_height = m_processed_textures[0]->GetHeight();
			m_ref_image = (uint32_t*)malloc(m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
			m_processed_textures[0]->GetData(m_ref_image);
			histograms.clear();
			avg_histogram.clear();
			snrs.clear();
			avg_snr = 0.0f;
			std::vector<uint32_t*> frames;
			utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
			ImageAnalysis::AnalyzeImages(frames, m_ref_image_width, m_ref_image_height, histograms, avg_histogram, snrs, avg_snr);
			for (auto& frame : frames) {
				free(frame);
			}
		}
		uint32_t* data = (uint32_t*)malloc(m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
		m_processed_textures[0]->GetData(data);
		if (memcmp(m_ref_image, data, m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4) != 0) {
			memcpy(m_ref_image, data, m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
			histograms.clear();
			avg_histogram.clear();
			snrs.clear();
			avg_snr = 0.0f;
			std::vector<uint32_t*> frames;
			utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
			ImageAnalysis::AnalyzeImages(frames, m_ref_image_width, m_ref_image_height, histograms, avg_histogram, snrs, avg_snr);
			for (auto& frame : frames) {
				free(frame);
			}
		}
		free(data);

		ImGui::SeparatorText("Write Analysis to CSV");
		if (ImGui::Button("Save To")) {
			auto path = utils::SaveFileDialog(".", "Save Analysis CSV", "csv");
			write_success = utils::saveAnalysisCsv(path.c_str(), histograms, avg_histogram, snrs, avg_snr);
		}
		if (!write_success) {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error saving widths!");
		}

		ImGui::SeparatorText("Average Analysis");
		ImGui::Text("Average SNR: %.2f", avg_snr);
		auto size = ImGui::GetIO().DisplaySize;
		size.x = size.x / 1.3f;
		ImGui::PlotHistogram("Average Histogram", &avg_histogram[0], 256, 0, NULL, 0.0f, 1.0f, ImVec2(size.x, 300));
		ImGui::SeparatorText("Frame Analysis");
		for (int i = 0; i < histograms.size(); i++) {
			char label[256];
			sprintf(label, "Frame %d", i);
			ImGui::PlotHistogram(label, &histograms[i][0], 256, 0, NULL, 0.0f, 1.0f, ImVec2(size.x, 300));
			ImGui::Text("SNR: %.2f", snrs[i]);
		}
		ImGui::EndTabItem();
	}
}

void ImageSet::DisplayFeatureTrackingTab() {
	static std::vector<std::vector<std::vector<float>>> widths;
	static std::vector<std::vector<float>> manual_widths;
	static bool write_success = true;
	static std::chrono::time_point<std::chrono::system_clock> last_time = std::chrono::system_clock::now();
	if (ImGui::BeginTabItem("Feature Tracking")) {
		if (m_processed_textures.size() == 0) {
			ImGui::Text("No images loaded");
			ImGui::EndTabItem();
			return;
		}

		// Initialize point image
		if (m_point_image == NULL) {
			m_point_image = (uint32_t*)malloc(m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
			m_processed_textures[0]->GetData(m_point_image);
			m_point_texture.Load(m_point_image, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}

		// Update point image if it isn't the same size as the ref texture
		if (m_point_texture.GetWidth() * m_point_texture.GetHeight() != m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight()) {
			free(m_point_image);
			m_point_image = (uint32_t*)malloc(m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
			m_processed_textures[0]->GetData(m_point_image);
			m_point_texture.Load(m_point_image, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}

		// Update point image if it isn't the same as the ref texture
		uint32_t* data = (uint32_t*)malloc(m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
		m_processed_textures[0]->GetData(data);
		if (m_points.size() == 0 && memcmp(m_point_image, data, m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4) != 0) {
			memcpy(m_point_image, data, m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
			m_point_texture.Load(data, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}
		free(data);

		ImGui::BeginChild("Controls", ImVec2(250, 0), true);
		static bool manualMode = false;
		ImGui::Text("Mode:");
		ImGui::RadioButton("Manual", (int*)&manualMode, true);
		ImGui::SameLine();
		ImGui::RadioButton("Auto", (int*)&manualMode, false);
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::Text("Manual mode allows you to select points to track features. Auto mode will automatically detect cracks and track their widths.\nWe suggest cropping the infobar for automatic tracking.");
			ImGui::EndTooltip();
		}

		if (manualMode) {
			if (m_points.size() > 0 && ImGui::Button("Clear Selection")) m_points.clear();
			if (m_points.size() % 2 == 0 && m_points.size() > 0) {
				if (ImGui::Button("Track Features")) {
					std::vector<uint32_t*> frames;
					utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
					std::vector<std::vector<cv::Point2f>> tracked_points;
					manual_widths = FeatureTracker::TrackFeatures(frames, m_points, tracked_points, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
					memcpy(m_point_image, frames[0], m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
					m_point_texture.Load(frames[0], m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
					utils::LoadDataIntoTexturesAndFree(m_processed_textures, frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
					m_last_points = m_points;
					m_last_tracked_points = tracked_points;
					m_points.clear();
				}
			} else {
				ImGui::Text("Select an even number of points");
			}
		} else {
			if (ImGui::Button("Track Widths")) {
				std::vector<uint32_t*> frames;
				utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
				auto polygons = CrackDetector::DetectCracks(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
				widths = FeatureTracker::TrackCrackWidthProfiles(polygons); // Assuming m_widths is a member variable
				utils::LoadDataIntoTexturesAndFree(m_processed_textures, frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
			}
		}
		if (manual_widths.size() > 0 && manualMode) {
			if (ImGui::Button("Clear Widths")) {
				manual_widths.clear();
				m_last_points.clear();
				uint32_t* data = (uint32_t*)malloc(m_textures[0]->GetWidth() * m_textures[0]->GetHeight() * 4);
				for (int i = 0; i < m_processed_textures.size(); i++) {
					m_textures[i]->GetData(data);
					m_processed_textures[i]->Load(data, m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight());
				}
				free(m_point_image);
				m_point_image = (uint32_t*)malloc(m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
				memcpy(m_point_image, data, m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
				m_point_texture.Load(m_point_image, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
			}
			if (ImGui::Button("Save To")) {
				auto path = utils::SaveFileDialog(".", "Save Widths CSV", "csv");
				write_success = utils::WriteCSV(path.c_str(), widths);
				if (!write_success) {
					ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error saving widths!");
				}
			}
			ImGui::Text("Manual Widths:");
			for (int i = 0; i < manual_widths.size(); i++) {
				ImGui::Text("Frame %d:", i);
				for (int j = 0; j < manual_widths[i].size(); j++) {
					if (j % 4 != 3) ImGui::SameLine();
					ImGui::Text("%.2f", manual_widths[i][j]);
				}
			}
		}
		if (widths.size() > 0 && !manualMode) {
			if (ImGui::Button("Clear Widths")) {
				widths.clear();
			}
			if (ImGui::Button("Save To")) {
				auto path = utils::SaveFileDialog(".", "Save Widths CSV", "csv");
				write_success = utils::WriteCSV(path.c_str(), widths);
				if (!write_success) {
					ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error saving widths!");
				}
			}
		}
		ImGui::EndChild();

		// Image View
		ImGui::SameLine();
		ImGui::BeginChild("ImageView", ImVec2(0, 0), true);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::ImageButton("Processed Image", m_point_texture.GetID(), ImVec2((float)m_point_texture.GetWidth(), (float)m_point_texture.GetHeight()));
		ImGui::PopStyleVar(2);

		if (manualMode && ImGui::IsItemActive() && ImGui::IsItemHovered()) {
			const auto now = std::chrono::system_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count() > 250) {
				last_time = now;
				int coordX = (ImGui::GetMousePos().x - ImGui::GetItemRectMin().x);
				int coordY = (ImGui::GetMousePos().y - ImGui::GetItemRectMin().y);
				m_points.push_back(cv::Point2f(coordX, coordY));

				// where the user clicks, draw a red dot
				int size = 3;
				for (int i = coordX - size; i < coordX + size + 1; i++) {
					for (int j = coordY - size; j < coordY + size + 1; j++) {
						if (i < 0 || j < 0 || i >= m_processed_textures[0]->GetWidth() || j >= m_processed_textures[0]->GetHeight()) continue;
						m_point_image[j * m_processed_textures[0]->GetWidth() + i] = 0xFFFF0000;
					}
				}
				m_point_texture.Load(m_point_image, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
			}
		}
		ImGui::EndChild();

		ImGui::EndTabItem();
	}
}

void ImageSet::DisplayDeformationAnalysisTab() {
	static bool good = true;
	static std::vector<tile> output_tiles;
	static std::vector<Texture*> output_tile_textures;
	static int tile_size = 256;
	static int overlap = 0;
	if (ImGui::BeginTabItem("Deformation Analysis")) {
		ImGui::SliderInt("Tile Size", &tile_size, 1, 512);
		ImGui::SliderInt("Overlap", &overlap, 0, 128);
		if (ImGui::Button("Calculate Deformation")) {
			std::vector<uint32_t*> frames;
			utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
			good = DeformationAnalysisInterface::RunModel(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), tile_size, overlap, output_tiles);
			utils::LoadDataIntoTexturesAndFree(m_processed_textures, frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}

		if (!good)
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "Model exited with an error!");

		if (output_tile_textures.size() == 0) {
			for (int i = 0; i < output_tiles.size(); i++) {
				Texture* t = new Texture;
				t->Load((uint32_t*)output_tiles[i].data.data, output_tiles[i].data.rows, output_tiles[i].data.cols);
				output_tile_textures.push_back(t);
			}
		}
		ImGui::SeparatorText("output tiles");
		ImGui::NewLine();
		for (int i = 0; i < output_tile_textures.size(); i++) {
			if (i % 4 != 3) ImGui::SameLine();
			ImGui::Image(output_tile_textures[i]->GetID(), ImVec2(output_tiles[i].data.rows, output_tiles[i].data.rows));
		}
		ImGui::EndTabItem();
	}
}

