file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS *.cpp *.hpp *.h)

file(GLOB IMGUI_LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/libs/imgui/*.cpp"
    "${CMAKE_SOURCE_DIR}/libs/imgui/*.h"
)

add_executable(EnhancingDeformationAnalysisUI
    ${SOURCES}
    ../libs/glad/src/glad.c
    ${IMGUI_LIB_SOURCES}
)

target_include_directories(EnhancingDeformationAnalysisUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/glad/include
    ${CMAKE_SOURCE_DIR}/libs/imgui
    ${CMAKE_SOURCE_DIR}/libs/gif-h
    ${CMAKE_SOURCE_DIR}/libs/cppflow/include
)

message("-- Source dir: ${CMAKE_SOURCE_DIR}")

# For ImGui to not try and load OpenGL itself
target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# Set C++ standard to 20
target_compile_features(EnhancingDeformationAnalysisUI PRIVATE cxx_std_20)

# --- Options for Subdirectory Libraries (Set BEFORE add_subdirectory) ---
# GLFW
if(!WIN32)
    SET(GLFW_BUILD_X11 ON CACHE BOOL "Build GLFW for X11" FORCE)
    SET(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build GLFW for Wayland" FORCE)
endif()
SET(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW docs" FORCE)
SET(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples" FORCE)
SET(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests" FORCE)
SET(GLFW_INSTALL OFF CACHE BOOL "Install GLFW separately" FORCE) # We bundle it

# LibTIFF
SET(tiff-tools OFF CACHE BOOL "Build libtiff tools" FORCE)
SET(tiff-tests OFF CACHE BOOL "Build libtiff tests" FORCE)
SET(tiff-contrib OFF CACHE BOOL "Build libtiff contrib" FORCE)
SET(tiff-docs OFF CACHE BOOL "Build libtiff docs" FORCE)
# SET(tiff-install OFF CACHE BOOL "Install libtiff separately" FORCE) # Let our app install it

# cppflow (if it has options to disable examples/tests)
SET(BUILD_EXAMPLES OFF CACHE BOOL "Build cppflow examples" FORCE) # Common option name for subprojects

# GLFW (assuming it's built as a library)
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/glfw ${CMAKE_BINARY_DIR}/libs/glfw)
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE glfw)

# libtiff (assuming it's built as a library)
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/libtiff ${CMAKE_BINARY_DIR}/libs/libtiff)
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE tiff)

if(UI_INCLUDE_TENSORFLOW)
    message(STATUS "Including TensorFlow with cppflow")

    # TF_PLATFORM logic matches the setup_tensorflow.py script's output directory name
    if(APPLE)
        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64") # More robust check for arm64
            set(TF_PLATFORM "macos-arm64")
        else()
            set(TF_PLATFORM "macos-x86")
        endif()
    elseif(WIN32)
        set(TF_PLATFORM "windows")
    elseif(UNIX)
        set(TF_PLATFORM "linux")
    endif()

    set(TF_ROOT "${CMAKE_SOURCE_DIR}/libs/tensorflow")
    set(TF_INCLUDE_DIR "${TF_ROOT}/include")
    set(TF_LIB_DIR "${TF_ROOT}/lib")

    if(NOT EXISTS "${TF_INCLUDE_DIR}/tensorflow/c/c_api.h")
        message(FATAL_ERROR "TensorFlow C API headers not found at ${TF_INCLUDE_DIR}. Run setup_tensorflow.py.")
    endif()

    target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${TF_INCLUDE_DIR})
    find_library(TF_C_LIB NAMES tensorflow tensorflow.lib PATHS "${TF_LIB_DIR}" REQUIRED)
    find_library(TF_FRAMEWORK_LIB NAMES tensorflow_framework tensorflow_framework.lib PATHS "${TF_LIB_DIR}" REQUIRED)
    target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${TF_C_LIB} ${TF_FRAMEWORK_LIB})

    target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${TF_C_LIB} ${TF_FRAMEWORK_LIB})

    # Define for conditional compilation in C++ code
    target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE UI_INCLUDE_TENSORFLOW)
    list(APPEND BUNDLE_SEARCH_PATHS "${TF_LIB_DIR}") # For install(RUNTIME_DEPENDENCIES)
    if(WIN32 AND EXISTS "${TF_ROOT}") # For tensorflow.dll often in root of TF package on Windows
        list(APPEND BUNDLE_SEARCH_PATHS "${TF_ROOT}")
    endif()
endif()

# --- PyTorch (LibTorch) ---
if(UI_INCLUDE_PYTORCH)
    message(STATUS "Including PyTorch (LibTorch)")

    # LibTorch discovery:
    # Option 1: User sets Torch_DIR environment variable or -DTorch_DIR=<path_to_libtorch_cmake_config_dir>
    # Option 2: We define a default path based on where our setup_pytorch.py script places it.

    # Construct a potential default path for Torch_DIR based on a convention for setup_pytorch.py
    # This PT_PLATFORM_SUBDIR would be set by the setup_pytorch.py or in CI
    # Example: "libtorch-cpu-linux-x86_64" or "libtorch-cpu-macos-x86_64" etc.
    # For now, we'll rely on Torch_DIR being set externally or found if installed system-wide (less likely for libtorch)
    if(DEFINED ENV{Torch_DIR})
        set(Torch_DIR_INTERNAL "$ENV{Torch_DIR}")
    elseif(DEFINED Torch_DIR) # Allow -DTorch_DIR
        set(Torch_DIR_INTERNAL "${Torch_DIR}")
    else()
        # Attempt to construct a default path if a script convention is established
        # This part is tricky without knowing the exact output of setup_pytorch.py
        # Let's assume setup_pytorch.py creates libs/pytorch/libtorch and inside that is share/cmake/Torch
        set(DEFAULT_LIBTORCH_ROOT "${CMAKE_SOURCE_DIR}/libs/pytorch/libtorch") # Example convention
        if(EXISTS "${DEFAULT_LIBTORCH_ROOT}/share/cmake/Torch/TorchConfig.cmake")
            set(Torch_DIR_INTERNAL "${DEFAULT_LIBTORCH_ROOT}/share/cmake/Torch")
            message(STATUS "Using default LibTorch path: ${Torch_DIR_INTERNAL}")
        else()
            message(STATUS "Torch_DIR is not set and default path not found. PyTorch find_package might fail.")
            set(Torch_DIR_INTERNAL "") # Fallback, find_package might still find it if installed globally
        endif()
    endif()

    if(Torch_DIR_INTERNAL)
        find_package(Torch REQUIRED PATHS "${Torch_DIR_INTERNAL}" NO_DEFAULT_PATH)
    else()
        find_package(Torch REQUIRED) # Try system paths if no specific path given
    endif()

    if(Torch_FOUND)
        target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${TORCH_LIBRARIES})
        target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${TORCH_INCLUDE_DIRS})
        target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE UI_INCLUDE_PYTORCH)

        # For bundling, TORCH_INSTALL_PREFIX/lib is usually where shared libs are.
        if(EXISTS "${TORCH_INSTALL_PREFIX}/lib")
            list(APPEND BUNDLE_SEARCH_PATHS "${TORCH_INSTALL_PREFIX}/lib")
        else()
            message(WARNING "TORCH_INSTALL_PREFIX/lib not found. LibTorch DLLs/SOs may not be bundled correctly.")
        endif()
    else()
        message(FATAL_ERROR "LibTorch (PyTorch C++ API) not found. Set Torch_DIR or ensure it's discoverable.")
    endif()
endif()

# OpenCV
find_package(OpenCV REQUIRED) # OpenCV_DIR might be set by CI script
target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_LIBS})

# --- UI_PROFILE Definition ---
if(UI_PROFILE)
    message(STATUS "-- Building in PROFILE mode (UI_PROFILE defined)")
    target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE UI_PROFILE)
endif()

# --- Installation ---
if(APPLE)
    set_target_properties(EnhancingDeformationAnalysisUI PROPERTIES
        MACOSX_BUNDLE TRUE
        # MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist" # Create and specify your Info.plist
        # MACOSX_BUNDLE_ICON_FILE "myapp.icns" # Create and specify your app icon
    )
    install(TARGETS EnhancingDeformationAnalysisUI BUNDLE DESTINATION . COMPONENT applications)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
        DESTINATION "${PROJECT_NAME}.app/Contents/MacOS/assets"
        COMPONENT applications
        FILES_MATCHING PATTERN "*"
    )
else()
    install(TARGETS EnhancingDeformationAnalysisUI RUNTIME DESTINATION bin)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/ DESTINATION assets)
endif()


# Bundling dependencies (requires CMake 3.21+)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    message(STATUS "Using CMake ${CMAKE_VERSION} with install(RUNTIME_DEPENDENCIES)")

    if(WIN32)
        install(
            TARGETS EnhancingDeformationAnalysisUI
            RUNTIME_DEPENDENCIES
            DESTINATION bin # DLLs next to exe
            # DIRECTORIES ${BUNDLE_SEARCH_PATHS}
        )
    elseif(APPLE)
        # Apple code
    else() # Linux
        install(
            TARGETS EnhancingDeformationAnalysisUI
            RUNTIME_DEPENDENCIES
            DESTINATION lib # Shared libraries in lib/
            # DIRECTORIES ${BUNDLE_SEARCH_PATHS}
        )

        set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

        set_target_properties(EnhancingDeformationAnalysisUI PROPERTIES
            INSTALL_RPATH "$ORIGIN/../lib"
            BUILD_RPATH "$ORIGIN/../lib") # For finding them during build if necessary
    endif()

    if(APPLE)

        if(EXISTS "/opt/homebrew/lib/gcc/current")
            message(STATUS "Adding Homebrew GCC lib dir to bundle search paths: ${HOMEBREW_GCC_LIB_DIR}")
            list(APPEND BUNDLE_SEARCH_PATHS "/opt/homebrew/lib/gcc/current")
        else()
            message(WARNING "Could not determine Homebrew GCC library path for bundling libgcc. This might lead to issues.")
        endif()

        if(EXISTS "/opt/homebrew/lib")
            message(STATUS "Adding Homebrew lib dir to bundle search paths: ${HOMEBREW_LIB_DIR}")
            list(APPEND BUNDLE_SEARCH_PATHS "/opt/homebrew/lib")
        else()
            message(WARNING "Could not determine Homebrew lib directory for bundling. This might lead to issues.")
        endif()

        string(JOIN ";" BUNDLE_SEARCH_PATHS_MACOS_STR ${BUNDLE_SEARCH_PATHS})

        install(CODE "
            include(BundleUtilities)
            message(STATUS \"FixupBundle: App - \${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app\")
            message(STATUS \"FixupBundle: Dirs - ${BUNDLE_SEARCH_PATHS_MACOS_STR}\")
            fixup_bundle(
                \"\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app\"
                \"\"
                \"${BUNDLE_SEARCH_PATHS_MACOS_STR}\"
            )
        "            COMPONENT applications)
        install(CODE "
            message(STATUS \"Ad-hoc signing the application bundle: \${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app\")
            execute_process(
                COMMAND codesign --force --deep --sign - \"\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app\"
                RESULT_VARIABLE sign_result
                OUTPUT_VARIABLE sign_output
                ERROR_VARIABLE sign_error
            )
            if(NOT sign_result EQUAL 0)
                message(FATAL_ERROR \"Failed to ad-hoc sign the bundle: \${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app. Output: \${sign_output}. Error: \${sign_error}\")
            else()
                message(STATUS \"Bundle ad-hoc signed successfully: \${sign_output}\")
            endif()
        "            COMPONENT applications)
    endif()

else()
    message(WARNING "CMake version is ${CMAKE_VERSION}. CMake 3.21+ is required for robust install(RUNTIME_DEPENDENCIES). Manual bundling of shared libraries will be attempted but may be incomplete.")
    # Minimal manual bundling for TF (already partially there from my previous suggestion)
    # Manual bundling for PyTorch and OpenCV would be very complex here and error-prone.
    # Strongly recommend ensuring CMake 3.21+ in all build environments.
    if(UI_INCLUDE_TENSORFLOW AND EXISTS "${TF_LIB_DIR}")
        if(WIN32)
            if(EXISTS "${TF_ROOT}/tensorflow.dll")
                install(FILES "${TF_ROOT}/tensorflow.dll" DESTINATION bin COMPONENT Runtime)
            endif()
            # You might need to glob for other TF related DLLs if any.
        elseif(APPLE) # Non-bundle scenario; .app bundle handled by fixup_bundle
            install(DIRECTORY "${TF_LIB_DIR}/" DESTINATION lib COMPONENT Runtime FILES_MATCHING PATTERN "libtensorflow*.dylib")
        else() # Linux
            install(DIRECTORY "${TF_LIB_DIR}/" DESTINATION lib COMPONENT Runtime FILES_MATCHING PATTERN "libtensorflow*.so*")
        endif()
    endif()
    # Add similar manual install for PyTorch and OpenCV if CMake < 3.21, which is non-trivial.
endif()
