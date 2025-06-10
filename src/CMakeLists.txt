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
)

message("-- Source dir: ${CMAKE_SOURCE_DIR}")

# For ImGui to not try and load OpenGL itself
target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# Set C++ standard to 20
target_compile_features(EnhancingDeformationAnalysisUI PRIVATE cxx_std_20)
if(APPLE)
  target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE "-framework CoreFoundation")
endif()

# --- Options for Subdirectory Libraries (Set BEFORE add_subdirectory) ---
# GLFW
if(!WIN32)
  set(GLFW_BUILD_X11 ON CACHE BOOL "Build GLFW for X11" FORCE)
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build GLFW for Wayland" FORCE)
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW docs" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Install GLFW separately" FORCE)

# LibTIFF
set(tiff-tools OFF CACHE BOOL "Build libtiff tools" FORCE)
set(tiff-tests OFF CACHE BOOL "Build libtiff tests" FORCE)
set(tiff-contrib OFF CACHE BOOL "Build libtiff contrib" FORCE)
set(tiff-docs OFF CACHE BOOL "Build libtiff docs" FORCE)

# cppflow (if it has options to disable examples/tests)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build cppflow examples" FORCE) # Common option name for subprojects

# GLFW
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/glfw ${CMAKE_BINARY_DIR}/libs/glfw)
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE glfw)

# libtiff
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/libtiff ${CMAKE_BINARY_DIR}/libs/libtiff)
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE tiff)

if(UI_INCLUDE_TENSORFLOW)
  message(STATUS "Including TensorFlow with cppflow")

  set(TF_ROOT "${CMAKE_SOURCE_DIR}/libs/tensorflow")
  set(TF_INCLUDE_DIR "${TF_ROOT}/include")
  set(TF_LIB_DIR "${TF_ROOT}/lib")

  if(NOT EXISTS "${TF_INCLUDE_DIR}/tensorflow/c/c_api.h")
    message(FATAL_ERROR "TensorFlow C API headers not found at ${TF_INCLUDE_DIR}. Run setup_tensorflow.py.")
  endif()

  target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${CMAKE_SOURCE_DIR}/libs/cppflow/include)
  target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${TF_INCLUDE_DIR})

  find_library(TF_C_LIB NAMES tensorflow tensorflow.lib PATHS "${TF_LIB_DIR}" REQUIRED)
  if(NOT WIN32)
    find_library(TF_FRAMEWORK_LIB NAMES tensorflow_framework tensorflow_framework.lib PATHS "${TF_LIB_DIR}" REQUIRED)
    target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${TF_FRAMEWORK_LIB})
  endif()


  target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${TF_C_LIB})

  # Define for conditional compilation in C++ code
  target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE UI_INCLUDE_TENSORFLOW)
  list(APPEND BUNDLE_SEARCH_PATHS "${TF_LIB_DIR}")

  if(WIN32 AND EXISTS "${TF_ROOT}") # For tensorflow.dll often in root of TF package on Windows
    list(APPEND BUNDLE_SEARCH_PATHS "${TF_ROOT}")
  endif()
endif()

# --- PyTorch (LibTorch) ---
if(UI_INCLUDE_PYTORCH)
  message(STATUS "Including PyTorch (LibTorch)")

  if(DEFINED ENV{Torch_DIR})
    set(Torch_DIR_INTERNAL "$ENV{Torch_DIR}")
  elseif(DEFINED Torch_DIR) # Allow -DTorch_DIR
    set(Torch_DIR_INTERNAL "${Torch_DIR}")
  else()
    set(DEFAULT_LIBTORCH_ROOT "${CMAKE_SOURCE_DIR}/libs/pytorch/libtorch")
    if(EXISTS "${DEFAULT_LIBTORCH_ROOT}/share/cmake/Torch/TorchConfig.cmake")
      set(Torch_DIR_INTERNAL "${DEFAULT_LIBTORCH_ROOT}/share/cmake/Torch")
      message(STATUS "Using default LibTorch path: ${Torch_DIR_INTERNAL}")
    else()
      message(STATUS "Torch_DIR is not set and default path not found. PyTorch find_package might fail.")
      set(Torch_DIR_INTERNAL "")
    endif()
  endif()

  if(Torch_DIR_INTERNAL)
    find_package(Torch REQUIRED PATHS "${Torch_DIR_INTERNAL}" NO_DEFAULT_PATH)
  else()
    find_package(Torch REQUIRED)
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
set(OLD_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Hint to find_package(OpenCV) to prefer shared libs" FORCE)
find_package(OpenCV REQUIRED)
target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_LIBS})

set(BUILD_SHARED_LIBS ${OLD_BUILD_SHARED_LIBS})

message(STATUS "OpenCV version: ${OpenCV_VERSION} at ${OpenCV_DIR}")

# Add OpenCV lib dir to BUNDLE_SEARCH_PATHS if needed for bundling
if(WIN32 AND OpenCV_DIR)
  list(APPEND BUNDLE_SEARCH_PATHS "${OpenCV_DIR}/bin")
  list(APPEND BUNDLE_SEARCH_PATHS "${OpenCV_DIR}/build")
  list(APPEND BUNDLE_SEARCH_PATHS "${OpenCV_DIR}/x64/vc16/bin")
endif()

# --- UI_PROFILE Definition ---
if(UI_PROFILE)
  message(STATUS "-- Building in PROFILE mode (UI_PROFILE defined)")
  target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE UI_PROFILE)
endif()

# ==================================================================================
# --- Installation, Bundling, and Signing (Conditional on APP_BUILD_MODE) ---
# ==================================================================================

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  message(STATUS "Configuring for Release. Will compile and bundle.")

  message(STATUS "Using CMake ${CMAKE_VERSION} with install(RUNTIME_DEPENDENCIES)")

  # ----------------------------------------------------
  #       Windows specific
  # ----------------------------------------------------

  if(WIN32)
    install(TARGETS EnhancingDeformationAnalysisUI
      RUNTIME DESTINATION bin COMPONENT applications
      LIBRARY DESTINATION lib COMPONENT applications
      ARCHIVE DESTINATION lib COMPONENT applications
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/ DESTINATION assets COMPONENT applications)

    set(CMAKE_RUNTIME_DEPENDENCY_RESOLUTION_SKIP_MISSING TRUE)

    install(
      TARGETS EnhancingDeformationAnalysisUI
      RUNTIME_DEPENDENCIES
      DIRECTORIES ${BUNDLE_SEARCH_PATHS}
      PRE_EXCLUDE_REGEXES
      # System API sets and forwarders
      "^api-ms-win-.*\\.dll$"
      "^ext-ms-.*\\.dll$"

      # Core Windows Media Foundation - DO NOT BUNDLE
      "^mfplat\\.dll$"
      "^mfcore\\.dll$"
      # Add any other MF related DLLs if they appear (e.g., mferror.dll, mfreadwrite.dll)
      # Generally, anything starting with "mf" that isn't from a third-party library you explicitly use.

      # Other problematic system/dev DLLs
      "^hvsimobilecore\\.dll$" # Corrected from HvsiFileTrust if that was a typo, or add both
      "^hvsifiletrust\\.dll$"
      "^wpaxholder\\.dll$"
      "^pdmutilities\\.dll$" # From previous fix

      # Standard C/C++ Runtimes (usually handled by VCRedist)
      "^ucrtbase\\.dll$"
      "^vcruntime[0-9]+\\.dll$"
      "^msvcp[0-9]+\\.dll$"
      DESTINATION bin
      COMPONENT applications
    )

    install(
      CODE
      "
        include(InstallRequiredSystemLibraries)
        include(BundleUtilities)
        fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.zip\" \"\" \"\")
    "
      DESTINATION .
      COMPONENT Runtime)

    # ----------------------------------------------------
    #       Apple Specific
    # ----------------------------------------------------
  elseif(APPLE)
    # Apple code

    set_target_properties(EnhancingDeformationAnalysisUI PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_ICON_FILE "EnhancingDeformationAnalysisUI.icns"
      MACOSX_BUNDLE_NAME "EnhancingDeformationAnalysisUI"
      MACOSX_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.enhancingdeformationanalysis.ui"
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/assets/Info.plist"
      XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "XVAD456636"
      XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Developer ID Application"
      XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
      XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED TRUE
      XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--timestamp=http://timestamp.apple.com/ts01  --options=runtime,library"
      XCODE_ATTRIBUTE_CODE_SIGN_INJECT_BASE_ENTITLEMENTS "NO"
    )
    set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "XVAD456636")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Developer ID Application: Aidan Schmitigal (XVAD456636)")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED TRUE)
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/assets/entitlements.plist")

    install(FILES ${CMAKE_SOURCE_DIR}/assets/EnhancingDeformationAnalysisUI.icns DESTINATION "${PROJECT_NAME}.app/Contents/Resources" COMPONENT applications)
    install(FILES ${CMAKE_SOURCE_DIR}/assets/Info.plist DESTINATION "${PROJECT_NAME}.app/Contents" COMPONENT applications)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/assets/EnhancingDeformationAnalysisUI.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    install(TARGETS EnhancingDeformationAnalysisUI
      BUNDLE DESTINATION .
      COMPONENT applications
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
      DESTINATION "${PROJECT_NAME}.app/Contents/MacOS/assets"
      COMPONENT applications
      FILES_MATCHING PATTERN "*"
    )

    if(EXISTS "/opt/homebrew/lib/gcc/current")
      message(STATUS "Adding Homebrew GCC lib dir to bundle search paths: ${HOMEBREW_GCC_LIB_DIR}")
      list(APPEND BUNDLE_SEARCH_PATHS "/opt/homebrew/lib/gcc/current")
    else()
      message(WARNING "Could not determine Homebrew GCC library path for bundling libgcc. This might lead to issues.")
    endif()

    if(BUNDLE_SEARCH_PATHS)
      list(REMOVE_DUPLICATES BUNDLE_SEARCH_PATHS)
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
        "      COMPONENT applications)
    install(CODE "
            message(STATUS \"Signing app with identity: ${CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY}\")
            execute_process(
                COMMAND codesign --force --deep --verify --verbose --timestamp --options runtime
                                --entitlements \"${CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS}\"
                                --sign \"${CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY}\"
                                \"\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app\"
                RESULT_VARIABLE sign_result
                OUTPUT_VARIABLE sign_output
                ERROR_VARIABLE sign_error
            )
            if(NOT sign_result EQUAL 0)
                message(FATAL_ERROR \"Code signing failed: \${sign_error}\")
            else()
                message(STATUS \"App signed successfully: \${sign_output}\")
            endif()
        "      COMPONENT applications)

    # ----------------------------------------------------
    #       Linux specific
    # ----------------------------------------------------
  elseif(UNIX) # Linux
    install(TARGETS EnhancingDeformationAnalysisUI
      RUNTIME DESTINATION bin COMPONENT applications
      LIBRARY DESTINATION lib COMPONENT applications
      ARCHIVE DESTINATION lib COMPONENT applications
    )
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/ DESTINATION assets COMPONENT applications)
    install(
      TARGETS EnhancingDeformationAnalysisUI
      RUNTIME_DEPENDENCIES
      DESTINATION lib COMPONENT applications
    )

    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

    set_target_properties(EnhancingDeformationAnalysisUI PROPERTIES
      INSTALL_RPATH "$ORIGIN/../lib"
      BUILD_RPATH "$ORIGIN/../lib")
  endif()
else() # CMAKE_BUILD_TYPE = "Debug"
  message(STATUS "Debug mode: Skipping installation, bundling, and CPack steps for src target.")
  # In development mode, the executable is just in ${CMAKE_BINARY_DIR}/src/
  # Users run it from there. RPATHs set by CMake for the build tree should handle finding
  # libraries from other build subdirectories (like glfw, libtiff) or system paths.
  # For pre-downloaded TF/PyTorch, you might need to ensure their lib dirs are in
  # the RPATH of the development executable or in DYLD_LIBRARY_PATH/LD_LIBRARY_PATH
  # or PATH (for Windows DLLs) when running.

  # Set RPATHs for development build to find libs downloaded by setup scripts
  # This helps run directly from the build tree without installing
  set(DEV_RPATH_DIRS "")
  if(UI_INCLUDE_TENSORFLOW AND TF_LIB_DIR AND EXISTS "${TF_LIB_DIR}")
    list(APPEND DEV_RPATH_DIRS "${TF_LIB_DIR}")
  endif()
  if(UI_INCLUDE_PYTORCH AND Torch_FOUND AND TORCH_INSTALL_PREFIX AND EXISTS "${TORCH_INSTALL_PREFIX}/lib")
    list(APPEND DEV_RPATH_DIRS "${TORCH_INSTALL_PREFIX}/lib")
  endif()

  # Also add paths to compiled subproject libraries (glfw, libtiff)
  # These are usually handled by CMake's default RPATH settings for build tree,
  # but being explicit doesn't hurt if issues arise.
  # list(APPEND DEV_RPATH_DIRS "${CMAKE_BINARY_DIR}/libs/glfw") # Example, if glfw lib is there
  # list(APPEND DEV_RPATH_DIRS "${CMAKE_BINARY_DIR}/libs/libtiff")

  if(DEV_RPATH_DIRS)
    if(APPLE)
      set_target_properties(EnhancingDeformationAnalysisUI PROPERTIES
        BUILD_WITH_INSTALL_RPATH ON # Uses INSTALL_RPATH also for build tree
        INSTALL_RPATH "${DEV_RPATH_DIRS}"
      )
    else() # Linux - $ORIGIN is useful
      # Convert DEV_RPATH_DIRS to $ORIGIN-relative if possible, or absolute paths
      string(REPLACE ";" "$ORIGIN/../lib${CMAKE_DIR_SEPARATOR}" RPATH_LINKS "${DEV_RPATH_DIRS}") # Simplistic
      set_target_properties(EnhancingDeformationAnalysisUI PROPERTIES
        BUILD_RPATH "${RPATH_LINKS}"
      )
    endif()
  endif()

  # For finding assets in DEVELOPMENT mode:
  # Your C++ code needs a way to find assets.
  # For example, if executable is build/src/YourApp and assets are YourProject/assets/
  # it might need to go up (../../assets) or use a path relative to CMAKE_SOURCE_DIR
  # during development. The getResourcesPath() needs a robust fallback for this.
  # Example: define a preprocessor macro with asset path for dev.
  target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE
    "APP_DEVELOPMENT_ASSET_PATH=\"${CMAKE_SOURCE_DIR}/assets\""
  )

endif()
