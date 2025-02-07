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

int main() {
#ifdef WIN32
	_putenv_s("TF_ENABLE_ONEDNN_OPTS", "0");
#else
	//setenv("TF_ENABLE_ONEDNN_OPTS", "0", 1);
	//setenv("CUDA_VISIBLE_DEVICES", "-1", 1);
#endif

	if (!glfwInit()) {
		printf("Failed to initialize GLFW\n");
		return -1;
	}

	GLFWwindow* window = glfwCreateWindow(1400, 1050, "Enhancing Deformation Analysis UI", NULL, NULL);
	glfwMakeContextCurrent(window);
	int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	if (!status) {
		printf("Failed to initialize OpenGL context\n");
		return -1;
	}

	ImGuiInit(window);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	std::vector<ImageSet*> image_sets;
	while (!glfwWindowShouldClose(window)) {
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glfwPollEvents();

		ImGuiBeginFrame();

		ImGui::DockSpaceOverViewport();
		ImGui::ShowDemoWindow();

		// for each image set, create a window that will be tabbed in the main window
		// for each image set tab, have tabs for stabilization and preprocessing etc.
		ImGui::Begin("Image Folder Selector");
		if (ImGui::Button("Select Folder")) {
			std::string folder_path = utils::OpenFileDialog(".", true);
			if (!folder_path.empty() && std::filesystem::is_directory(folder_path)) {
				image_sets.emplace_back(new ImageSet(folder_path));
			}
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
