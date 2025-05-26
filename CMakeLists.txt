cmake_minimum_required(VERSION 3.21) # Consider 3.21 for install(RUNTIME_DEPENDENCIES)
project(EnhancingDeformationAnalysisUI VERSION 0.2 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH}) # Add local cmake modules path

option(UI_INCLUDE_TENSORFLOW "Include TensorFlow in the project" ON)
option(UI_INCLUDE_PYTORCH "Include PyTorch in the project" OFF) # Unused in this context
option(UI_PROFILE "Build the project in profile mode" OFF) # Unused in this context

add_subdirectory(src)

# CPack configuration
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Enhancing Deformation Analysis UI Application")
set(CPACK_PACKAGE_VENDOR "Your Organization") # Replace with your info
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt") # Create a LICENSE.txt
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_NAME} v${PROJECT_VERSION}")
    set(CPACK_NSIS_DISPLAY_NAME "${PROJECT_NAME}")
    # set(CPACK_NSIS_INSTALLER_ICON_CODE "${CMAKE_SOURCE_DIR}/assets/icons/app_icon.ico") # Example icon
    # set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${PROJECT_NAME}.exe")
    # set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE)
elseif(APPLE)
    set(CPACK_GENERATOR "DMG")
    # For a .app bundle, ensure MACOSX_BUNDLE is set on the target in src/CMakeLists.txt
    # And install the target using BUNDLE DESTINATION.
    # set(CPACK_DMG_ também_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/assets/img/dmg-background.png") # Example background
else() # Linux
    set(CPACK_GENERATOR "TGZ;DEB") # You might want to choose one, or provide both
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Your Name <your.email@example.com>") # Replace
    set(CPACK_RPM_PACKAGE_RELEASE "1")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Science")
endif()

set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};ALL;/")

include(CPack)