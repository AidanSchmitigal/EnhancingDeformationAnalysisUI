#include <stdio.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>

#include <ImGuiImpl.h>
#include <ui/ImageSequenceViewer.h>

int main() {
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

	ImageSequenceViewer viewer;
	while (!glfwWindowShouldClose(window)) {
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glfwPollEvents();

		ImGuiBeginFrame();
		ImGui::DockSpaceOverViewport();

		ImGui::Begin("Hello, World!");
		ImGui::Text("Hello, World!");
		ImGui::End();

		viewer.Display();

		ImGuiEndFrame();

		glfwSwapBuffers(window);
	}
	return 0;
}
