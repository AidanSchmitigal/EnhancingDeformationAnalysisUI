cmake_minimum_required(VERSION 3.5)

project(EnhancingDeformationAnalysisUI VERSION 0.1
	DESCRIPTION "UI for the Enhancing Deformation Analysis capstone"
	LANGUAGES CXX C)

# Enable compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set the source directory
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Find all .cpp files in the source directory and its subdirectories
file(GLOB_RECURSE SOURCES "${SRC_DIR}/**.cpp" "${SRC_DIR}/**.h")

SET(GLFW_BUILD_X11 ON CACHE BOOL "Build for X11")
SET(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build for Wayland")

find_package(OpenCV REQUIRED)

add_subdirectory(libs/glfw)

# Don't let glfw build examples/tests/docs or install
SET(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW lib only")
SET(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW lib only")
SET(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW lib only")
SET(GLFW_INSTALL OFF CACHE BOOL "GLFW lib only")

add_executable(EnhancingDeformationAnalysisUI ${SOURCES} libs/stb_image/stb_image_write.h libs/stb_image/stb_image.h libs/glad/src/glad.c libs/imgui/imgui.cpp libs/imgui/imgui_draw.cpp libs/imgui/imgui_widgets.cpp libs/imgui/imgui_tables.cpp libs/stb_image/stb_image.cpp libs/stb_image/stb_image_write.cpp src/utils.h src/utils.cpp)

# For imgui to not try and load OpenGL itself
target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# Link to glfw
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE glfw)

target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE tiff)

# Include directories
target_include_directories(EnhancingDeformationAnalysisUI PRIVATE src/ libs/ libs/glad/include/ libs/imgui/)

target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_INCLUDE_DIRS})

# Set C++ standard to 20
target_compile_features(EnhancingDeformationAnalysisUI PRIVATE cxx_std_20)

target_compile_options(EnhancingDeformationAnalysisUI PRIVATE -Wall -O2)

# Link to python
target_compile_options(EnhancingDeformationAnalysisUI PRIVATE -L/usr/lib/python3.12/config-3.12-x86_64-linux-gnu -L/usr/lib/x86_64-linux-gnu -lpython3.12 -ldl -lm)
