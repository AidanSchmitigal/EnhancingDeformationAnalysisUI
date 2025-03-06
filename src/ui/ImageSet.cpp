#include <ui/ImageSet.h>

#include <core/stabilizer.hpp>
#include <core/CrackDetector.hpp>
#include <core/DenoiseInterface.hpp>
#include <core/FeatureTracker.hpp>
#include <core/DeformationAnalysisInterface.hpp>
#include <core/ImageAnalysis.hpp>

#include <utils.h>

#include <imgui.h>

#include <opencv2/opencv.hpp>

#include <cppflow/cppflow.h>

#include <string>
#include <filesystem>

int ImageSet::m_id_counter = 0;

int playspeed = 1;

ImageSet::ImageSet(const std::string_view &folder_path) : m_folder_path(folder_path) {
	m_window_name = "ImageSet " + std::to_string(m_id_counter++);

	LoadImages();

	m_point_texture = Texture();
	m_sequence_viewer = ImageSequenceViewer(m_textures, "Original Images");
	m_processed_sequence_viewer = ImageSequenceViewer(m_processed_textures, "Processed Images");

	m_preprocessing_tab = PreprocessingTab(m_textures, m_processed_textures);
}

ImageSet::~ImageSet() {
	for (auto& texture : m_textures) {
		delete texture;
	}
	for (auto& texture : m_processed_textures) {
		delete texture;
	}
	free(m_point_image);
}

void ImageSet::Display() {
	ImGui::Begin(m_window_name.c_str(), &m_open);
	ImGui::BeginTabBar("PreProcessing");

	bool changed = false;
	// TODO: refactor all of these into separate files
	DisplayImageComparisonTab();
	m_preprocessing_tab.DisplayPreprocessingTab(changed);
	DisplayImageAnalysisTab();
	DisplayFeatureTrackingTab();
	DisplayDeformationAnalysisTab();

	// only necessary when we modify the vector by changing its size (this only happens in the preprocessing tab)
	if (changed) {
		m_preprocessing_tab.GetProcessedTextures(m_processed_textures);
		m_processed_sequence_viewer.SetTextures(m_processed_textures);
	}

	// TODO: move this somewhere else
	if (ImGui::BeginTabItem("Save Images")) {
		if (ImGui::Button("Save Processed Images")) {
			std::string folder = utils::OpenFileDialog(".", "Choose Where to Save the Images", true);
			if (!folder.empty()) {
				for (int i = 0; i < m_processed_textures.size(); i++) {
					char path[256];
					sprintf(path, "%s/frame_%d.tif", folder.c_str(), i);
					uint32_t* data = new uint32_t[m_processed_textures[i]->GetWidth() * m_processed_textures[i]->GetHeight()];
					m_processed_textures[i]->GetData(data);
					utils::WriteTiff(path, data, m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight());
					free(data);
				}
			}
		}
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

	// find all .tif files in the folder
	std::vector<std::string> files;
	for (const auto& entry : std::filesystem::directory_iterator(m_folder_path)) {
		if (entry.path().string().find(".tif") == std::string::npos)
			continue;
		files.push_back(entry.path().string());
	}

	// sort the files by name
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

void ImageSet::DisplayImageComparisonTab() {
	if (ImGui::BeginTabItem("Image Comparison")) {
		if (ImGui::Button((m_sequence_viewer.GetPlaying() && m_processed_sequence_viewer.GetPlaying()) ? "Stop Both" : "Play Both")) {
			m_sequence_viewer.StartStopPlay();
			m_processed_sequence_viewer.StartStopPlay();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Frame Counters")) {
			m_sequence_viewer.SetCurrentFrame(0);
			m_processed_sequence_viewer.SetCurrentFrame(0);
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
			m_preprocessing_tab.SetProcessedTextures(m_processed_textures);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetIO().DisplaySize.x / 2.2);
		ImGui::SliderInt("Images / Second", &playspeed, 1, 10);
		m_sequence_viewer.SetPlaySpeed(playspeed);
		m_processed_sequence_viewer.SetPlaySpeed(playspeed);
		m_sequence_viewer.Display();
		ImGui::SameLine();
		m_processed_sequence_viewer.Display();
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
		if (m_point_image == nullptr) {
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
			if (ImGui::Button("Clear Selection")) m_points.clear();
			if (m_points.size() % 2 == 0 && m_points.size() > 0) {
				if (ImGui::Button("Track Features")) {
					std::vector<uint32_t*> frames;
					utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
					std::vector<std::vector<cv::Point2f>> tracked_points;
					manual_widths = FeatureTracker::TrackFeatures(frames, m_points, tracked_points, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
					memcpy(m_point_image, frames[0], m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
					m_point_texture.Load(frames[0], m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
					utils::LoadDataIntoTexturesAndFree(m_processed_textures, frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
					m_points.clear();
				}
			} else {
				ImGui::Text("Select two points");
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
			}
			static std::string folder_path;
			if (ImGui::Button("Choose Folder to Save")) {
				folder_path = utils::OpenFileDialog(".", "Pick a folder to save widths", true);
			}
			static char manual_filename[256] = "";
			ImGui::InputTextWithHint("Filename", "widths.csv", manual_filename, 256);
			ImGui::TextWrapped("%s/%s", folder_path.c_str(), manual_filename);
			if (ImGui::Button("Save Widths")) {
				write_success = utils::WriteCSV(std::string(folder_path + "/" + manual_filename).c_str(), manual_widths);
				if (!write_success) {
					ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error saving widths!");
				}
				memset(manual_filename, 0, 256);
				folder_path = "";
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
			static std::string folder_path;
			if (ImGui::Button("Choose Folder to Save")) {
				folder_path = utils::OpenFileDialog(".", "Pick a folder to save widths", true);
			}
			static char filename[256] = "";
			ImGui::InputTextWithHint("Filename", "widths.csv", filename, 256);
			ImGui::TextWrapped("%s/%s", folder_path.c_str(), filename);
			if (ImGui::Button("Save Widths")) {
				write_success = utils::WriteCSV(std::string(folder_path + "/" + filename).c_str(), widths);
				if (!write_success) {
					ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error saving widths!");
				}
				memset(filename, 0, 256);
				folder_path = "";
			}
		}
		ImGui::EndChild();

		// Image View
		ImGui::SameLine();
		ImGui::BeginChild("ImageView", ImVec2(0, 0), true);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::ImageButton("Processed Image", m_point_texture.GetID(), ImVec2(m_point_texture.GetWidth(), m_point_texture.GetHeight()));
		ImGui::PopStyleVar(2);

		if (manualMode && ImGui::IsItemActive() && ImGui::IsItemHovered()) {
			const auto now = std::chrono::system_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count() > 250) {
				last_time = now;
				uint coordX = (ImGui::GetMousePos().x - ImGui::GetItemRectMin().x);
				uint coordY = (ImGui::GetMousePos().y - ImGui::GetItemRectMin().y);
				m_points.push_back(cv::Point2f(coordX, coordY));

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
	if (ImGui::BeginTabItem("Deformation Analysis")) {
		if (ImGui::Button("Calculate Deformation")) {
			std::vector<uint32_t*> frames;
			utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
			good = DeformationAnalysisInterface::TestModel(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), 256, 0);
			utils::LoadDataIntoTexturesAndFree(m_processed_textures, frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}
		if (!good)
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "Model exited with an error!");
		ImGui::EndTabItem();
	}
}

