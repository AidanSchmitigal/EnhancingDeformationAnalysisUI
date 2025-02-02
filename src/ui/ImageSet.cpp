#include <ui/ImageSet.h>

#include <ui/ImageSequenceViewer.h>
#include <utils.h>

#include <imgui.h>

#include <string>
#include <filesystem>

int ImageSet::m_id_counter = 0;

ImageSet::ImageSet(const std::string_view &folder_path) : m_folder_path(folder_path) {
	m_window_name = "ImageSet " + std::to_string(m_id_counter++);

	LoadImages();

	m_sequence_viewer = ImageSequenceViewer(m_textures);
	m_processed_sequence_viewer = ImageSequenceViewer(m_processed_textures);
}

void ImageSet::Display() {
	ImGui::Begin(m_window_name.c_str());
	ImGui::BeginTabBar("PreProcessing");

	if (ImGui::BeginTabItem("Image Comparison")) {
		ImGui::NewLine();
		m_sequence_viewer.Display();
		ImGui::SameLine();
		m_processed_sequence_viewer.Display();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Stabilization")) {
		ImGui::Text("Stabilization tab");
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Preprocessing")) {
		ImGui::Text("Preprocessing tab");
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Deformation Analysis")) {
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
		int copy_width = width;
		int copy_height = height;
		Texture* t = new Texture();
		t->Load(temp, width, height);
		m_textures.push_back(t);
		Texture* t2 = new Texture();
		t2->Load(temp, copy_width, copy_height);
		m_processed_textures.push_back(t2);
		free(temp);
	}
}
