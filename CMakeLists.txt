cmake_minimum_required(VERSION 3.5)

project(EnhancingDeformationAnalysisUI VERSION 0.1
	DESCRIPTION "UI for the Enhancing Deformation Analysis capstone"
	LANGUAGES CXX C)

# Enable compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libs/cppflow/cmake/modules)
# Set the source directory
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Find all .cpp files in the source directory and its subdirectories
file(GLOB_RECURSE SOURCES "${SRC_DIR}/**.cpp" "${SRC_DIR}/**.h")

file(GLOB IMGUI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui/*.cpp")

SET(GLFW_BUILD_X11 OFF CACHE BOOL "Build for X11")
SET(GLFW_BUILD_WAYLAND ON CACHE BOOL "Build for Wayland")

find_package(OpenCV REQUIRED)

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

add_subdirectory(libs/glfw)
add_subdirectory(libs/pybind11)
add_subdirectory(libs/libtiff)
add_subdirectory(libs/cppflow)

# Don't let glfw build examples/tests/docs or install
SET(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW lib only")
SET(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW lib only")
SET(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW lib only")
SET(GLFW_INSTALL OFF CACHE BOOL "GLFW lib only")

add_executable(EnhancingDeformationAnalysisUI ${SOURCES} libs/stb_image/stb_image_write.h libs/stb_image/stb_image.h libs/glad/src/glad.c ${IMGUI_SOURCES} libs/stb_image/stb_image.cpp libs/stb_image/stb_image_write.cpp)

# For imgui to not try and load OpenGL itself
target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# Link to glfw
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE glfw)

target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE tiff)

target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE cppflow)

target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_LIBS})

target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${Python3_LIBRARIES})

# Include directories
target_include_directories(EnhancingDeformationAnalysisUI PRIVATE src/ libs/ libs/glad/include/ libs/imgui/)

# Include OpenCV
target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_INCLUDE_DIRS})

target_include_directories(EnhancingDeformationAnalysisUI PRIVATE libs/pybind11/include)

target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${Python3_INCLUDE_DIRS})

target_include_directories(EnhancingDeformationAnalysisUI PRIVATE libs/cppflow/include)

# Set C++ standard to 20
target_compile_features(EnhancingDeformationAnalysisUI PRIVATE cxx_std_20)

if (UI_RELEASE)
	target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE UI_RELEASE)
endif()
if (WIN32)
	# enabling optimizations leads to issues when using visual studio, it conflicts with /RTC1
	# to enable optimizations, create a release configuration and build from there
else()
	target_compile_options(EnhancingDeformationAnalysisUI PRIVATE -O2)
endif()
