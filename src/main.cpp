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

#include <cli.hpp>

int main(int argc, char** argv) {
#ifndef UI_RELEASE
	std::filesystem::current_path("../");
#endif // UI_RELEASE

	// if there's any args, run the cli instead
	if (argc > 1) {
		cli::run(argc, argv);
		return 0;
	}

	// confirm we have the assets folder (ai models and config)
	bool assets_folder_exists = std::filesystem::exists("assets");
	if (!assets_folder_exists) {
		fprintf(stderr, "ERROR: Assets folder not found! The folder is required for this program to function correctly!\n");
	}

	if (!glfwInit()) {
		printf("Failed to initialize GLFW\n");
		return -1;
	}

#ifdef __APPLE__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // for macOS
#endif

#ifdef UI_RELEASE
	GLFWwindow* window = glfwCreateWindow(1400, 1050, "Enhancing Deformation Analysis UI", NULL, NULL);
#else
	GLFWwindow* window = glfwCreateWindow(1400, 1050, "Enhancing Deformation Analysis UI (DEBUG)", NULL, NULL);
#endif

	glfwMakeContextCurrent(window);
	int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	if (!status) {
		printf("Failed to initialize OpenGL context\n");
		printf("This is likely due to an issue with the graphics driver/card\n");
		printf("If you do not have a supported display adapter (e.g. if you are using a VM), use the command line interface instead\n");
		return -1;
	}

	// set glfw error callback, because we might as well
	glfwSetErrorCallback([](int error, const char* description) {
			fprintf(stderr, "GLFW Error %d: %s\n", error, description);
			});

	bool set_colors = true;
	// call ImGuiInit to initialize the ImGui context
	// this will also install glfw callbacks
	ImGuiInit(window, set_colors, assets_folder_exists);

	// default to dark theme if no custom theme
	if (!set_colors)
		ImGui::StyleColorsDark();

	// allow docking
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// set glfw to use vsync, results in less cpu usage and no visible difference
	glfwSwapInterval(1);

	// create a vector of ImageSet pointers to store the image sets
	std::vector<ImageSet*> image_sets;
	while (!glfwWindowShouldClose(window)) {
		// set the default background and clear the screen
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// check for events
		glfwPollEvents();

		// begin the ImGui frame
		ImGuiBeginFrame();

		// render the dockspace for all the other windows
		ImGuiID dockspaceID = ImGui::DockSpaceOverViewport();

		// show the demo window if in debug
#ifndef UI_RELEASE
		ImGui::ShowDemoWindow();
#endif
		
		// is this necessary when we use the SetNextWindowDockID function?
		// TODO: test
		if (!std::filesystem::exists("imgui.ini"))
		{
			ImGui::LoadIniSettingsFromDisk("assets/DefaultLayout.ini");
		}

		ImGui::SetNextWindowDockID(dockspaceID, ImGuiCond_FirstUseEver);

#ifdef __APPLE__
		ImGui::Begin("Image Folder Selector");
		ImGui::InputTextWithHint("##image_folder", "Enter image folder path", nullptr, 0);
		if (ImGui::Button("Load Images")) {
			const char* folder_path = ImGui::GetInputTextState("##image_folder")->Text;
			if (std::filesystem::is_directory(folder_path)) {
				image_sets.emplace_back(new ImageSet(folder_path));
			}
		}
#else
		// for each image set, create a window that will be tabbed in the main window
		// for each image set tab, have tabs for stabilization and preprocessing etc.
		ImGui::Begin("Image Folder Selector");
		if (ImGui::Button("Select Folder")) {
			std::string folder_path = utils::OpenFileDialog(".", "Choose a Folder to Load", true);
			if (!folder_path.empty() && std::filesystem::is_directory(folder_path)) {
				image_sets.emplace_back(new ImageSet(folder_path));
			}
		}

#endif

		// display a warning if the assets folder is not found
		if (!assets_folder_exists) {
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Assets folder not found! Please place the assets folder in the same directory as the executable.");
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "The assets folder is required for the program to function correctly.");
		}

		ImGui::End();

		// display the image sets
		for (int i = 0; i < image_sets.size(); i++) {
			// if the image set is closed, delete it and remove it from the vector
			if (image_sets[i]->Closed()) {
				delete image_sets[i];
				image_sets.erase(image_sets.begin() + i);
				i--;
				continue;
			}
			ImGui::SetNextWindowDockID(dockspaceID, ImGuiCond_FirstUseEver);
			image_sets[i]->Display();
		}

		// end the ImGui frame
		ImGuiEndFrame();

		glfwSwapBuffers(window);
	}

	glfwDestroyWindow(window);
	glfwTerminate(); // this segfaults on wayland? (using glfw from master branch)
	return 0;
}
