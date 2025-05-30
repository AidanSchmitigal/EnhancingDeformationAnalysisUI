cmake_minimum_required(VERSION 3.21)
project(EnhancingDeformationAnalysisUI VERSION 0.2 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_SOURCE_DIR}/libs/cppflow/cmake/modules ${CMAKE_MODULE_PATH})

message(STATUS "CMake Build Type: ${CMAKE_BUILD_TYPE}")

option(UI_INCLUDE_TENSORFLOW "Include TensorFlow in the project" ON)
option(UI_INCLUDE_PYTORCH "Include PyTorch in the project" ON)
option(UI_RELEASE "Build the project in release mode" ON)
option(UI_PROFILE "Build the project in profile mode" OFF)

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

  if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_NAME} v${PROJECT_VERSION}")
    set(CPACK_NSIS_DISPLAY_NAME "${PROJECT_NAME}")
    # set(CPACK_NSIS_INSTALLER_ICON_CODE "${CMAKE_SOURCE_DIR}/assets/icons/app_icon.ico") # Example icon
    # set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${PROJECT_NAME}.exe")
    # set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE)
  elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    # For a .app bundle, ensure MACOSX_BUNDLE is set on the target in src/CMakeLists.txt
    # And install the target using BUNDLE DESTINATION.
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/assets/img/dmg-background.png") # Example background
  else() # Linux
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Your Name <your.email@example.com>")
    set(CPACK_RPM_PACKAGE_RELEASE "1")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Science")
  endif()

  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
  set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};ALL;/")

  include(CPack)
else()
  message(STATUS "CPack is disabled for APP_BUILD_MODE: ${APP_BUILD_MODE}")
endif()
