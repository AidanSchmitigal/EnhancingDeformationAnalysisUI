#include <filesystem>
#include <stdio.h>
#include <cstdlib>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>

#include <ImGuiImpl.h>
#include <ui/ImageSequenceViewer.h>
#include <ui/ImageSet.h>
#include <utils.h>

int main(int argc, char** argv) {
#ifdef WIN32
	_putenv_s("TF_ENABLE_ONEDNN_OPTS", "1");
#ifndef UI_RELEASE
	std::filesystem::current_path("../");
#endif
#else
#ifndef UI_RELEASE
	std::filesystem::current_path("../");
#endif
	setenv("TF_ENABLE_ONEDNN_OPTS", "1", 1);
#endif

	// TODO: prepare for command line usage, no GUI
	
	bool assets_folder_exists = std::filesystem::exists("assets");
	if (!assets_folder_exists) {
		fprintf(stderr, "ERROR: Assets folder not found! The folder is required for this program to function correctly!\n");
	}

	if (!glfwInit()) {
		printf("Failed to initialize GLFW\n");
		return -1;
	}

#ifdef UI_RELEASE
	GLFWwindow* window = glfwCreateWindow(1400, 1050, "Enhancing Deformation Analysis UI", NULL, NULL);
#else
	GLFWwindow* window = glfwCreateWindow(1400, 1050, "Enhancing Deformation Analysis UI (DEBUG)", NULL, NULL);
#endif

	glfwMakeContextCurrent(window);
	int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	if (!status) {
		printf("Failed to initialize OpenGL context\n");
		return -1;
	}

	ImGuiInit(window, false, assets_folder_exists);
	ImGui::StyleColorsDark();

	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	std::vector<ImageSet*> image_sets;
	while (!glfwWindowShouldClose(window)) {
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glfwPollEvents();

		ImGuiBeginFrame();

		ImGui::DockSpaceOverViewport();

#ifndef UI_RELEASE
		ImGui::ShowDemoWindow();
#endif

		// for each image set, create a window that will be tabbed in the main window
		// for each image set tab, have tabs for stabilization and preprocessing etc.
		ImGui::Begin("Image Folder Selector");
		if (ImGui::Button("Select Folder")) {
			std::string folder_path = utils::OpenFileDialog(".", true);
			if (!folder_path.empty() && std::filesystem::is_directory(folder_path)) {
				image_sets.emplace_back(new ImageSet(folder_path));
			}
		}
		if (!assets_folder_exists) {
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Assets folder not found! Please place the assets folder in the same directory as the executable.");
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "The assets folder is required for the program to function correctly.");
		}

		ImGui::End();

		for (int i = 0; i < image_sets.size(); i++) {
			if (image_sets[i]->Closed()) {
				delete image_sets[i];
				image_sets.erase(image_sets.begin() + i);
				i--;
				continue;
			}
			image_sets[i]->Display();
		}

		ImGuiEndFrame();

		glfwSwapBuffers(window);
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
