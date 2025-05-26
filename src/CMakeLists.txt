file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS *.cpp *.hpp *.h)

add_executable(EnhancingDeformationAnalysisUI ${SOURCES} ../libs/glad/src/glad.c)

target_include_directories(EnhancingDeformationAnalysisUI PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/glad/include
    ${CMAKE_SOURCE_DIR}/libs/imgui
    ${CMAKE_SOURCE_DIR}/libs/gif-h
    # If cppflow is header only and in libs/cppflow
    ${CMAKE_SOURCE_DIR}/libs/cppflow 
)

message("-- Source dir: ${CMAKE_SOURCE_DIR}")

# GLFW (assuming it's built as a library)
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/glfw ${CMAKE_BINARY_DIR}/libs/glfw)
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE glfw)

# libtiff (assuming it's built as a library)
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/libtiff ${CMAKE_BINARY_DIR}/libs/libtiff)
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE tiff)

# ImGui
file(GLOB IMGUI_SRC CONFIGURE_DEPENDS 
    "${CMAKE_SOURCE_DIR}/libs/imgui/*.cpp" 
    "${CMAKE_SOURCE_DIR}/libs/imgui/*.h"
    # Ensure your imgui_impl files are also included if not in SOURCES
    # The glob recurse already picks up imgui_impl_glfw.cpp and imgui_impl_opengl3.cpp from src
)
# target_sources(EnhancingDeformationAnalysisUI PRIVATE ${IMGUI_SRC}) # This is not needed if already in SOURCES
target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# OpenCV
find_package(OpenCV REQUIRED) # OpenCV_DIR might be set by CI script
target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${OpenCV_LIBS})

if (UI_INCLUDE_TENSORFLOW)
    message(STATUS "Including TensorFlow")

    # TF_PLATFORM logic matches the setup_tensorflow.py script's output directory name
    if(APPLE)
        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64") # More robust check for arm64
            set(TF_PLATFORM "macos-arm64")
        else()
            set(TF_PLATFORM "macos-x86")
        endif()
    elseif(WIN32)
        set(TF_PLATFORM "windows")
    elseif(UNIX) # Linux
        set(TF_PLATFORM "linux")
    endif()

    set(TF_ROOT "${CMAKE_SOURCE_DIR}/libs/tensorflow/${TF_PLATFORM}")
    set(TF_INCLUDE_DIR "${TF_ROOT}/include")
    set(TF_LIB_DIR "${TF_ROOT}/lib")

    if(NOT EXISTS "${TF_ROOT}/include/tensorflow/c/c_api.h")
        message(FATAL_ERROR "TensorFlow C API not found at ${TF_ROOT}. Run the setup_tensorflow.py script.")
    endif()

    target_include_directories(EnhancingDeformationAnalysisUI PRIVATE ${TF_INCLUDE_DIR})
    
    # TensorFlow libraries linking
    # For Windows, tensorflow.lib is in TF_LIB_DIR. tensorflow_framework.lib might also be there.
    # For Linux/macOS, libtensorflow.so/dylib and libtensorflow_framework.so/dylib are in TF_LIB_DIR.
    find_library(TF_C_LIB NAMES tensorflow tensorflow.lib PATHS "${TF_LIB_DIR}" REQUIRED)
    find_library(TF_FRAMEWORK_LIB NAMES tensorflow_framework tensorflow_framework.lib PATHS "${TF_LIB_DIR}" REQUIRED)
    
    target_link_libraries(EnhancingDeformationAnalysisUI PRIVATE ${TF_C_LIB} ${TF_FRAMEWORK_LIB})
    
    # Define for conditional compilation in C++ code
    target_compile_definitions(EnhancingDeformationAnalysisUI PRIVATE UI_INCLUDE_TENSORFLOW)
endif()

# --- Installation ---
install(TARGETS EnhancingDeformationAnalysisUI
    RUNTIME DESTINATION bin
    # For macOS .app bundle:
    # BUNDLE DESTINATION . COMPONENT applications 
    # Make sure to set MACOSX_BUNDLE properties if you want an .app bundle
)
# Example for macOS .app bundle (requires an Info.plist file)
# if(APPLE)
#   set_target_properties(EnhancingDeformationAnalysisUI PROPERTIES
#     MACOSX_BUNDLE TRUE
#     MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist" 
#     # MACOSX_BUNDLE_ICON_FILE "myapp.icns"
#   )
# endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/ DESTINATION assets)

# Bundling dependencies (requires CMake 3.21+)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    message(STATUS "Using CMake ${CMAKE_VERSION} with install(RUNTIME_DEPENDENCIES)")
    set(BUNDLE_SEARCH_PATHS "")
    if(UI_INCLUDE_TENSORFLOW AND EXISTS "${TF_LIB_DIR}")
        list(APPEND BUNDLE_SEARCH_PATHS "${TF_LIB_DIR}")
        if(WIN32 AND EXISTS "${TF_ROOT}") # For tensorflow.dll in TF_ROOT on Windows
            list(APPEND BUNDLE_SEARCH_PATHS "${TF_ROOT}")
        endif()
    endif()
    if(OpenCV_FOUND AND OpenCV_LIB_DIR) # OpenCV_LIB_DIR might not be standard. OpenCV_LIBS contains paths.
                                        # Better to rely on system paths or OpenCV_DIR to find DLLs/SOs.
        # For Windows, OpenCV DLLs are often in a 'bin' directory alongside 'lib'
        if(WIN32 AND OpenCV_DIR)
            # Try to find the bin directory of OpenCV installation
            # This path can vary depending on how OpenCV was installed/downloaded.
            # For official release, it is often ${OpenCV_DIR}/../../bin
            # For choco/vcpkg, the path would be different and OpenCV_DIR might point to CMake configs.
            # It's safer to ensure OpenCV bin dir is in PATH for find_package and let install(RUNTIME_DEPENDENCIES) find it.
            # list(APPEND BUNDLE_SEARCH_PATHS "${OpenCV_DIR}/../../bin") # Adjust path as needed
        endif()
    endif()

    if(WIN32)
        install(
            TARGETS EnhancingDeformationAnalysisUI
            RUNTIME_DEPENDENCIES
            DESTINATION bin # DLLs next to exe
            # DIRECTORIES ${BUNDLE_SEARCH_PATHS}
        )
    else() # macOS and Linux
        install(
            TARGETS EnhancingDeformationAnalysisUI
            RUNTIME_DEPENDENCIES
            DESTINATION lib # Shared libraries in lib/
            # DIRECTORIES ${BUNDLE_SEARCH_PATHS}
        )
        # Set RPATH for the executable to find these libraries
        set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
        if(APPLE)
            # If creating an .app bundle, fixup_bundle (BundleUtilities) is preferred and handles RPATHs internally.
            # If not an .app bundle:
            set_target_properties(EnhancingDeformationAnalysisUI PROPERTIES INSTALL_RPATH "@executable_path/../lib")
        else() # Linux
            set_target_properties(EnhancingDeformationAnalysisUI PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
        endif()
    endif()
else()
    message(WARNING "CMake version is ${CMAKE_VERSION}. CMake 3.21+ is recommended for install(RUNTIME_DEPENDENCIES). Manual bundling logic might be needed as a fallback or use BundleUtilities.cmake.")
    # Add manual installation of TF libs here if CMake < 3.21
    if (UI_INCLUDE_TENSORFLOW)
        if(WIN32)
            install(FILES "${TF_ROOT}/tensorflow.dll" DESTINATION bin COMPONENT Runtime)
        elseif(APPLE)
            install(DIRECTORY "${TF_LIB_DIR}/" DESTINATION lib COMPONENT Runtime FILES_MATCHING PATTERN "libtensorflow*.dylib")
             set_target_properties(EnhancingDeformationAnalysisUI PROPERTIES INSTALL_RPATH "@executable_path/../lib")
        else() # Linux
            install(DIRECTORY "${TF_LIB_DIR}/" DESTINATION lib COMPONENT Runtime FILES_MATCHING PATTERN "libtensorflow*.so*")
            set_target_properties(EnhancingDeformationAnalysisUI PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
        endif()
    endif()
    # Manual bundling for OpenCV would also be needed here.
    # For macOS, you could use BundleUtilities.cmake:
    # if(APPLE)
    #   include(BundleUtilities)
    #   add_custom_command(TARGET EnhancingDeformationAnalysisUI POST_BUILD
    #     COMMAND ${CMAKE_COMMAND} -E médicas_copy_directory ${OpenCV_ nécessaire_dylibs_dir} $<TARGET_FILE_DIR:EnhancingDeformationAnalysisUI>/../Frameworks/ # Example
    #   )
    #   install(CODE "
    #       include(BundleUtilities)
    #       fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/bin/EnhancingDeformationAnalysisUI\" \"\" \"${TF_LIB_DIR};${OpenCV_LIB_DIR_OR_SIMILAR}\")
    #   ")
    # endif()
endif()