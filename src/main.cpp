#include <filesystem>
#include <stdio.h>
#include <cstdlib>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>

#include <ImGuiImpl.h>

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
		printf("If you do not have a supported display adapter (e.g., if you are using a VM), use the command line interface instead\n");
		return -1;
	}

	// set glfw error callback, because we might as well
	glfwSetErrorCallback([](int error, const char* description) {
			fprintf(stderr, "GLFW Error %d: %s\n", error, description);
			});

	constexpr bool set_colors = true;
	// call ImGuiInit to initialize the ImGui context
	// this will also install glfw callbacks
	ImGuiInit(window, set_colors, assets_folder_exists);

	// default to dark theme if no custom theme
	if (!set_colors)
		ImGui::StyleColorsDark();

	// allow docking
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// set glfw to use vsync, results in less cpu usage and no visible difference
	// this limits the frame rate to the refresh rate of the monitor
	glfwSwapInterval(1);

	// create a vector of ImageSet pointers to store the image sets
	std::vector<ImageSet*> image_sets;
	
	// State for welcome UI
	static bool show_welcome = true;
	
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
		
		ImGui::SetNextWindowDockID(dockspaceID, ImGuiCond_FirstUseEver);

		// for each image set, create a window that will be tabbed in the main window
		// for each image set tab, have tabs for stabilization and preprocessing etc.
		ImGui::Begin("Image Folder Selector");
		
		// Enhanced UI with welcome section
		if (show_welcome) {
			// Styled title
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.56f, 1.0f, 1.0f));
			ImGui::SetWindowFontScale(1.5f);
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Deformation Analysis UI").x) * 0.5f);
			ImGui::Text("Deformation Analysis UI");
			ImGui::SetWindowFontScale(1.0f);
			ImGui::PopStyleColor();
			ImGui::PopFont();
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			// Description
			ImGui::TextWrapped("This application helps analyze deformation in microscopy images using AI-powered tools.");
			ImGui::Spacing();
			
			// Key features
			ImGui::TextColored(ImVec4(0.28f, 0.56f, 1.0f, 1.0f), "Key Features:");
			ImGui::Bullet(); ImGui::TextWrapped("Deformation tracking and analysis");
			ImGui::Bullet(); ImGui::TextWrapped("Image stabilization");
			ImGui::Bullet(); ImGui::TextWrapped("Feature tracking across image sequences");
			ImGui::Bullet(); ImGui::TextWrapped("AI-powered crack detection");
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			// Getting started
			ImGui::TextColored(ImVec4(0.28f, 0.56f, 1.0f, 1.0f), "Getting Started:");
			ImGui::TextWrapped("1. Click the 'Select Folder' button below to load your TIFF images");
			ImGui::TextWrapped("2. Each image set will open in a new tab");
			ImGui::TextWrapped("3. Use the tabs within each image set for stabilization, preprocessing, and analysis");
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() * 0.25f);
			if (ImGui::Button("Hide Welcome Screen", ImVec2(ImGui::GetWindowWidth() * 0.5f, 0))) {
				show_welcome = false;
			}
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::GetItemRectSize().x) * 0.5f);
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
		}
		
#ifdef __APPLE__
		ImGui::InputTextWithHint("##image_folder", "Enter image folder path", nullptr, 0);
		if (ImGui::Button("Load Images")) {
			const char* folder_path = ImGui::GetInputTextState("##image_folder")->Text;
			if (std::filesystem::is_directory(folder_path)) {
				image_sets.emplace_back(new ImageSet(folder_path));
			}
		}
#else
		// Make the folder selector button more prominent
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.56f, 1.0f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.56f, 1.0f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.56f, 1.0f, 1.0f));
		
		// Center the button horizontally
		float buttonWidth = 150.0f;
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
		
		if (ImGui::Button("Select Folder", ImVec2(buttonWidth, 35))) {
			std::string folder_path = utils::OpenFileDialog(".", "Choose a Folder to Load", true);
			if (!folder_path.empty() && std::filesystem::is_directory(folder_path) && utils::DirectoryContainsTiff(folder_path)) {
				image_sets.emplace_back(new ImageSet(folder_path));
			}
		}
		
		ImGui::PopStyleColor(3);
#endif

		// display a warning if the assets folder is not found
		if (!assets_folder_exists) {
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "WARNING: Assets folder not found!");
			ImGui::TextWrapped("Please place the assets folder in the same directory as the executable. The assets folder is required for the program to function correctly.");
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
	glfwTerminate();
	return 0;
}
