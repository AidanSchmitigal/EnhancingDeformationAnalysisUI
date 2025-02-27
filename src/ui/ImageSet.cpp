#include <ui/ImageSet.h>

#include <ui/ImageSequenceViewer.h>
#include <core/stabilizer.hpp>
#include <core/CrackDetector.hpp>
#include <core/DenoiseInterface.hpp>
#include <core/FeatureTracker.hpp>
#include <core/DeformationAnalysisInterface.hpp>
#include <utils.h>

#include <imgui.h>

#include <opencv2/opencv.hpp>

#include <cppflow/cppflow.h>

#include <string>
#include <filesystem>
#include <unordered_map>

int ImageSet::m_id_counter = 0;

int playspeed = 1;
std::unordered_map<int, int> selected_textures_map;

ImageSet::ImageSet(const std::string_view &folder_path) : m_folder_path(folder_path) {
	m_window_name = "ImageSet " + std::to_string(m_id_counter++);

	LoadImages();

	m_point_texture = Texture();
	m_sequence_viewer = ImageSequenceViewer(m_textures, "Original Images");
	m_processed_sequence_viewer = ImageSequenceViewer(m_processed_textures, "Processed Images");
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

	DisplayImageComparisonTab();
	DisplayPreprocessingTab();
	DisplayImageAnalysisTab();
	DisplayFeatureTrackingTab();
	DisplayDeformationAnalysisTab();
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

void ImageSet::DisplayPreprocessingTab() {
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
					selected_textures_map[i] = 1;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Deselect All")) {
				selected_textures_map.clear();
			}
			if (ImGui::Button("Remove Selected")) {
				std::vector<int> to_remove;
				for (auto& item : selected_textures_map) {
					to_remove.push_back(item.first);
				}
				std::sort(to_remove.begin(), to_remove.end());
				for (int i = to_remove.size() - 1; i >= 0; i--) {
					delete m_processed_textures[to_remove[i]];
					m_processed_textures.erase(m_processed_textures.begin() + to_remove[i]);
				}
				selected_textures_map.clear();
				m_processed_sequence_viewer.SetTextures(m_processed_textures);
			}
			int images_per_line = ImGui::GetIO().DisplaySize.x / 115;
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
				ImGui::Text("This is a very early version of the crack detection algorithm. It works best on images with high contrast between the cracks and the background. \n\n"
						"To use it, first denoise the images using the \"Denoising\" section. Then, click the \"Detect Cracks\" button. The algorithm will then process the images and display the results.");
				ImGui::EndTooltip();
			}
		}
		ImGui::EndTabItem();
	}
}

void ImageSet::DisplayImageAnalysisTab() {
	struct ImageStatsCache {
		double snr = 0.0;
		std::vector<float> histogram;
		bool computed = false; // Flag to avoid unnecessary recomputation
	};
	static std::vector<ImageStatsCache> imageStatsCache;
	static std::vector<float> avg_histogram;
	static float avg_snr = 0.0f;
	static bool avg_computed = false;
	if (ImGui::BeginTabItem("Image Analysis")) {
		if (ImGui::Button("Recalculate Stats")) {
			avg_histogram.clear();
			avg_snr = 0.0f;
			avg_computed = false;
			for (int i = 0; i < imageStatsCache.size(); i++) {
				imageStatsCache[i].computed = false;
			}
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::Text("To reduce memory usage, the image stats are shared between the image sets. If you want to recalculate the stats, click this button.");
			ImGui::EndTooltip();
		}
		if (avg_histogram.size() == 0) {
			avg_histogram.resize(256);
		}
		for (int i = 0; i < m_processed_textures.size(); i++) {	
			if (i >= imageStatsCache.size()) {
				imageStatsCache.push_back(ImageStatsCache());
			}
			if (imageStatsCache[i].computed) {
				continue;
			}
			imageStatsCache.push_back(ImageStatsCache());
			uint32_t* data = (uint32_t*)malloc(m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
			m_processed_textures[i]->GetData(data);
			cv::Mat img(m_processed_textures[i]->GetHeight(), m_processed_textures[i]->GetWidth(), CV_8UC4, data);
			cv::cvtColor(img, img, cv::COLOR_BGRA2GRAY);
			cv::Scalar mean, stddev;
			cv::meanStdDev(img, mean, stddev);
			cv::Scalar snr = mean[0] / stddev[0];
			imageStatsCache[i].snr = snr[0];
			avg_snr += snr[0];

			int bins = 256;
			imageStatsCache[i].histogram.resize(bins);
			cv::Mat hist;
			float range[] = {0, 256};
			const float* histRange = {range};
			bool uniform = true, accumulate = false;
			cv::calcHist(&img, 1, 0, cv::Mat(), hist, 1, &bins, &histRange, uniform, accumulate);
			// Normalize for display
			cv::normalize(hist, hist, 0, 1, cv::NORM_MINMAX);

			for (int j = 0; j < bins; j++) {
				imageStatsCache[i].histogram[j] = hist.at<float>(j);
				avg_histogram[j] += hist.at<float>(j);
			}

			imageStatsCache[i].computed = true;
			free(data);
		}
		if (!avg_computed) {
			avg_snr /= m_processed_textures.size();
			for (int i = 0; i < avg_histogram.size(); i++) {
				avg_histogram[i] /= m_processed_textures.size();
			}
			avg_computed = true;
		}

		ImGui::Text("Average SNR: %.2f", avg_snr);
		ImGui::PlotHistogram("Average Histogram", avg_histogram.data(), avg_histogram.size(), 0, NULL, 0, 1.0f, ImVec2(0, 80));
		for (int i = 0; i < imageStatsCache.size(); i++) {
			if (!imageStatsCache[i].computed)
				continue;
			ImGui::Text("SNR: %.2f", imageStatsCache[i].snr);
			char hist_text[100];
			snprintf(hist_text, 100, "Image %d", i);
			ImGui::PlotHistogram(hist_text, imageStatsCache[i].histogram.data(), imageStatsCache[i].histogram.size(), 0, NULL, 0, 1.0f, ImVec2(0, 80));
		}
		ImGui::EndTabItem();
	}
}

void ImageSet::DisplayFeatureTrackingTab() {
	static std::chrono::time_point<std::chrono::system_clock> last_time = std::chrono::system_clock::now();
	if (ImGui::BeginTabItem("Feature Tracking")) {
		if (m_point_image == nullptr) {
			m_point_image = (uint32_t*)malloc(m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
			m_processed_textures[0]->GetData(m_point_image);
			m_point_texture.Load(m_point_image, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}
		if (ImGui::Button("Clear Selection")) {
			if (m_point_texture.GetWidth() * m_point_texture.GetHeight() != m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight()) {
				free(m_point_image);
				m_point_image = (uint32_t*)malloc(m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
			}
			m_processed_textures[0]->GetData(m_point_image);
			m_point_texture.Load(m_point_image, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
			m_points.clear();
		}
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::ImageButton("Processed Image", m_point_texture.GetID(), ImVec2(m_point_texture.GetWidth(), m_point_texture.GetHeight()));
		ImGui::PopStyleVar(2);
		const auto now = std::chrono::system_clock::now();
		if (ImGui::IsItemActive() && ImGui::IsItemHovered() && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count() > 250) {
			last_time = now;
			uint coordX = (ImGui::GetMousePos().x - ImGui::GetItemRectMin().x);
			uint coordY = (ImGui::GetMousePos().y - ImGui::GetItemRectMin().y);
			m_points.push_back(cv::Point2f(coordX, coordY));

			// size of the point, 3 == 3x3
			int size = 3;
			for (int i = coordX - size; i < coordX + size + 1; i++) {
				for (int j = coordY - size; j < coordY + size + 1; j++) {
					if (i < 0 || j < 0 || i >= m_processed_textures[0]->GetWidth() || j >= m_processed_textures[0]->GetHeight()) {
						continue;
					}
					m_point_image[j * m_processed_textures[0]->GetWidth() + i] = 0xFFFF0000;
				}
			}
			m_point_texture.Load(m_point_image, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}
		if (m_points.size() % 2 == 0 && m_points.size() > 0) {
			if (ImGui::Button("Track Features")) {
				std::vector<uint32_t*> frames;
				for (int i = 0; i < m_processed_textures.size(); i++) {
					uint32_t* data = (uint32_t*)malloc(m_processed_textures[i]->GetWidth() * m_processed_textures[i]->GetHeight() * 4);
					m_processed_textures[i]->GetData(data);
					frames.push_back(data);
				}
				std::vector<std::vector<cv::Point2f>> tracked_points;
				FeatureTracker::TrackFeatures(frames, m_points, tracked_points, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
				memcpy(m_point_image, frames[0], m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
				m_point_texture.Load(frames[0], m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
				for (int i = 0; i < frames.size(); i++) {
					m_processed_textures[i]->Load(frames[i], m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight());
					free(frames[i]);
				}
				m_points.clear();
			}
		}
		ImGui::EndTabItem();
	}
}

void ImageSet::DisplayDeformationAnalysisTab() {
	if (ImGui::BeginTabItem("Deformation Analysis")) {
		if (ImGui::Button("Calculate Deformation")) {
			std::vector<uint32_t*> frames;
			for (int i = 0; i < m_processed_textures.size(); i++) {
				uint32_t* data = (uint32_t*)malloc(m_processed_textures[i]->GetWidth() * m_processed_textures[i]->GetHeight() * 4);
				m_processed_textures[i]->GetData(data);
				frames.push_back(data);
			}
			DeformationAnalysisInterface::TestModel(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), 256, 0);
			for (int i = 0; i < frames.size(); i++) {
				m_processed_textures[i]->Load(frames[i], m_processed_textures[i]->GetWidth(), m_processed_textures[i]->GetHeight());
				free(frames[i]);
			}
		}
		ImGui::EndTabItem();
	}
}

void ImageSet::DisplayTestTab() {
	if (ImGui::BeginTabItem("Test")) {
		ImGui::EndTabItem();
	}
}
