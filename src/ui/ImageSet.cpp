#include <ui/ImageSet.h>

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

std::vector<float> calculateCrackWidthProfile(const std::vector<cv::Point>& polygon) {
	std::vector<float> widths;
	if (polygon.size() < 3) return widths;

	int n = polygon.size();
	int sampleCount = std::max(5, n); // Sample ~20% of points, min 5
	for (int i = 0; i < n; i += std::max(1, n / sampleCount)) {
		cv::Point p1 = polygon[i];
		std::vector<float> distances;

		// Compute distances to all other points
		for (const auto& p2 : polygon) {
			float dist = cv::norm(p2 - p1);
			if (dist > 0) distances.push_back(dist);
		}

		// Take second smallest distance as width at this point
		if (distances.size() > 1) {
			std::sort(distances.begin(), distances.end());
			widths.push_back(distances[1]);
		}
	}

	return widths;
}

// Track width profiles for all cracks in all images
std::vector<std::vector<std::vector<float>>> trackCrackWidthProfiles(
		const std::vector<std::vector<std::vector<cv::Point>>>& polygons) {
	std::vector<std::vector<std::vector<float>>> profilesPerImage;

	for (const auto& imagePolygons : polygons) { // For each image
		std::vector<std::vector<float>> profiles;
		for (const auto& polygon : imagePolygons) { // For each crack
			profiles.push_back(calculateCrackWidthProfile(polygon));
		}
		profilesPerImage.push_back(profiles);
	}

	return profilesPerImage;
}

void ImageSet::DisplayFeatureTrackingTab() {
	static std::vector<std::vector<std::vector<float>>> widths;
	static bool widths_computed = false;
	static bool write_success = true;
	static std::chrono::time_point<std::chrono::system_clock> last_time = std::chrono::system_clock::now();
	if (ImGui::BeginTabItem("Feature Tracking")) {
		// Initialize point image if needed
		if (m_point_image == nullptr) {
			m_point_image = (uint32_t*)malloc(m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
			m_processed_textures[0]->GetData(m_point_image);
			m_point_texture.Load(m_point_image, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
		}
		if (widths.size() == 0)
			widths_computed = false;
		else
			widths_computed = true;

		ImGui::BeginChild("Controls", ImVec2(250, 0), true);
		static bool manualMode = false;
		ImGui::Text("Mode:");
		ImGui::RadioButton("Manual", (int*)&manualMode, true);
		ImGui::SameLine();
		ImGui::RadioButton("Auto", (int*)&manualMode, false);

		if (manualMode) {
			if (ImGui::Button("Clear Selection")) {
				if (m_point_texture.GetWidth() * m_point_texture.GetHeight() != m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight()) {
					free(m_point_image);
					m_point_image = (uint32_t*)malloc(m_processed_textures[0]->GetWidth() * m_processed_textures[0]->GetHeight() * 4);
				}
				m_processed_textures[0]->GetData(m_point_image);
				m_point_texture.Load(m_point_image, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
				m_points.clear();
			}
			if (m_points.size() % 2 == 0 && m_points.size() > 0) {
				if (ImGui::Button("Track Features")) {
					std::vector<uint32_t*> frames;
					utils::GetDataFromTextures(frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight(), m_processed_textures);
					std::vector<std::vector<cv::Point2f>> tracked_points;
					FeatureTracker::TrackFeatures(frames, m_points, tracked_points, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
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
				widths = trackCrackWidthProfiles(polygons); // Assuming m_widths is a member variable
				utils::LoadDataIntoTexturesAndFree(m_processed_textures, frames, m_processed_textures[0]->GetWidth(), m_processed_textures[0]->GetHeight());
			}
		}
		if (widths_computed) {
			if (ImGui::Button("Clear Widths")) {
				widths.clear();
				widths_computed = false;
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

void ImageSet::DisplayTestTab() {
	if (ImGui::BeginTabItem("Test")) {
		ImGui::EndTabItem();
	}
}
