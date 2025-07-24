cmake_minimum_required(VERSION 3.21)
project(EnhancingDeformationAnalysisUI VERSION 0.2 DESCRIPTION "UI for the Enhancing Deformation Analysis capstone" LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_SOURCE_DIR}/libs/cppflow/cmake/modules ${CMAKE_MODULE_PATH})

message(STATUS "CMake Build Type: ${CMAKE_BUILD_TYPE}")

option(UI_INCLUDE_TENSORFLOW "Include TensorFlow in the project" ON)
option(UI_INCLUDE_PYTORCH "Include PyTorch in the project" ON)
option(UI_RELEASE "Build the project in release mode" ON)
option(UI_PROFILE "Build the project in profile mode" OFF)

if(APPLE)

  add_subdirectory(src)

  # --- CPack configuration ---
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Configuring CPack for packaging.")

    set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Enhancing Deformation Analysis UI Application")
    set(CPACK_PACKAGE_VENDOR "OSU Capstone Project")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")

    file(READ "${CMAKE_SOURCE_DIR}/README.md" README_MD)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/README.txt" "${README_MD}")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_BINARY_DIR}/README.txt")

    set(CPACK_GENERATOR "DragNDrop")
    # For a .app bundle, ensure MACOSX_BUNDLE is set on the target in src/CMakeLists.txt
    # And install the target using BUNDLE DESTINATION.
    # set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/assets/img/dmg-background.png") # Example background

    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};ALL;/")

    include(CPack)
  else()
    message(STATUS "CPack is disabled for APP_BUILD_MODE: ${APP_BUILD_MODE}")
  endif()

else()

  set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
  # Find all .cpp files in the source directory and its subdirectories
  file(GLOB_RECURSE SOURCES "${SRC_DIR}/**.cpp" "${SRC_DIR}/**.h")

  file(GLOB IMGUI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui/*.cpp")


  if(!WIN32)
    set(GLFW_BUILD_X11 ON CACHE BOOL "Build for X11")
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build for Wayland")
  endif()

  set(BUILD_EXAMPLES OFF CACHE BOOL "cppflow lib only")
  set(tiff-tools OFF CACHE BOOL "libtiff lib only")
  set(tiff-tests OFF CACHE BOOL "libtiff lib only")
  set(tiff-contrib OFF CACHE BOOL "libtiff lib only")
  set(tiff-docs OFF CACHE BOOL "libtiff lib only")
  set(tiff-install OFF CACHE BOOL "libtiff lib only")

  find_package(OpenCV REQUIRED)

  add_subdirectory(libs/glfw)
  add_subdirectory(libs/libtiff)

  if(UI_INCLUDE_PYTORCH)
    find_package(Torch REQUIRED)
  endif()
  if(UI_INCLUDE_TENSORFLOW)
    add_subdirectory(libs/cppflow)
  endif()

  # Don't let glfw build examples/tests/docs or install
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW lib only")
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW lib only")
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW lib only")
  set(GLFW_INSTALL OFF CACHE BOOL "GLFW lib only")

  add_executable(EnhancingDeformationAnalysisUI ${SOURCES} libs/glad/src/glad.c ${IMGUI_SOURCES})

  # For imgui to not try and load OpenGL itself
  target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

  # Link to glfw
  target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE glfw)

  target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE tiff)

  # must link cppflow before opencv or you'll get a segfault apparently (???)
  if(UI_INCLUDE_TENSORFLOW)
    target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE cppflow)
    target_include_directories(EnhancingDeformationAnalysisUI PRIVATE libs/cppflow/include)
  endif()

  if(UI_INCLUDE_PYTORCH)
    target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${TORCH_LIBRARIES})
    target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${TORCH_INCLUDE_DIRS})
  endif()

  target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_LIBS})

  # Include directories
  target_include_directories(EnhancingDeformationAnalysisUI PRIVATE src/ libs/ libs/glad/include/ libs/imgui/ libs/gif-h/)

  # Include OpenCV
  target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_INCLUDE_DIRS})

  # Set C++ standard to 20
  target_compile_features(EnhancingDeformationAnalysisUI PRIVATE cxx_std_20)

  if(UI_INCLUDE_TENSORFLOW)
    target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE UI_INCLUDE_TENSORFLOW)
  endif()

  if(UI_INCLUDE_PYTORCH)
    target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE UI_INCLUDE_PYTORCH)
  endif()

  if(UI_PROFILE)
    message("-- Building in PROFILE mode")
    message("-- Recommended to release mode to get accurate profiling")
    target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE UI_PROFILE)
  endif()

  if(UI_RELEASE)
    message("-- Building in RELEASE mode")
    target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE UI_RELEASE)

    # Windows doesn't like setting the optimization level through CMake
    # TODO: investigate this
    if(!WIN32)
      target_compile_options(EnhancingDeformationAnalysisUI PRIVATE -O3)
    endif() # !WIN32

  else() # UI_RELEASE
    message("-- Building in DEBUG mode")
    if(!WIN32)
      target_compile_options(EnhancingDeformationAnalysisUI PRIVATE -O)
      target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE -g)
    endif() # !WIN32
  endif() # UI_RELEASE

  if(UI_INCLUDE_PYTORCH)
    message("-- Building with PyTorch support")
  endif()

  if(UI_INCLUDE_TENSORFLOW)
    message("-- Building with TensorFlow support")
    message("-- TensorFlow libraries: ${tensorflow_LIBRARIES}")
    message("-- TensorFlow include directories: ${tensorflow_INCLUDE_DIRS}")
  endif()


endif()
