file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS *.cpp *.hpp *.h)

add_executable(EnhancingDeformationAnalysisUI ${SOURCES} ../libs/glad/src/glad.c)

target_include_directories(EnhancingDeformationAnalysisUI PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/glad/include
    ${CMAKE_SOURCE_DIR}/libs/imgui
    ${CMAKE_SOURCE_DIR}/libs/gif-h
)

# Log the source dir
message("-- Source dir: ${CMAKE_SOURCE_DIR}")

add_subdirectory(${CMAKE_SOURCE_DIR}/libs/glfw ${CMAKE_BINARY_DIR}/libs/glfw)
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE glfw)

add_subdirectory(${CMAKE_SOURCE_DIR}/libs/libtiff ${CMAKE_BINARY_DIR}/libs/libtiff)
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE tiff)

# ImGui
file(GLOB IMGUI_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/libs/imgui/*.cpp" "${CMAKE_SOURCE_DIR}/libs/imgui/*.h")
target_sources(EnhancingDeformationAnalysisUI PRIVATE ${IMGUI_SRC})
target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# OpenCV
find_package(OpenCV REQUIRED)
target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_LIBS})

# TensorFlow C API + cppflow
if (UI_INCLUDE_TENSORFLOW)
    set(TF_PLATFORM "")
    if (APPLE)
        set(TF_PLATFORM "macos")
    elseif (WIN32)
        set(TF_PLATFORM "windows")
    else()
        set(TF_PLATFORM "linux")
    endif()

    target_include_directories(EnhancingDeformationAnalysisUI PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/tensorflow/${TF_PLATFORM}/include
        ${CMAKE_SOURCE_DIR}/libs/cppflow/include
    )
    target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/tensorflow/${TF_PLATFORM}/libtensorflow.so
    )
endif()
