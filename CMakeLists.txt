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

    set(CPACK_NSIS_PUBLISHER "Your Organization") # Good to set
    set(CPACK_NSIS_URL_INFO_ABOUT "Your Project URL") # Good to set
    set(CPACK_NSIS_HELP_LINK "Your Help URL") # Optional
    set(CPACK_NSIS_CONTACT "Your Contact Email/User") # Optional

    # This tells NSIS to install all files from the "applications" component.
    # Ensure your install commands in src/CMakeLists.txt use COMPONENT ${APP_COMPONENT}
    # where APP_COMPONENT is "applications".
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64") # Install to Program Files (x64)
    # If your app is 32-bit, use $PROGRAMFILES instead.

    # Key part: Tell NSIS which components to install.
    # If all your files (bin, assets) are installed under the same component
    # (e.g., "applications" or the default "Unspecified"), list that component.
    # CPack will then look at CMAKE_INSTALL_PREFIX for files belonging to this component.
    set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "Application Files") # Name for the component in installer UI
    set(CPACK_COMPONENT_APPLICATIONS_DESCRIPTION "The main application files and associated data.")
    set(CPACK_COMPONENTS_ALL applications) # Install the 'applications' component

    # Create a Start Menu shortcut for the main executable
    # Make sure your executable is installed to <CMAKE_INSTALL_PREFIX>/bin/EnhancingDeformationAnalysisUI.exe
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
      "CreateShortCut '$SMPROGRAMS/${CPACK_NSIS_PACKAGE_NAME}.lnk' '$INSTDIR/bin/${PROJECT_NAME}.exe'")

    # Optionally, create a desktop shortcut
    # set(CPACK_NSIS_CREATE_ICONS_EXTRA 
    #   "${CPACK_NSIS_CREATE_ICONS_EXTRA}"
    #   "CreateShortCut '$DESKTOP\\${CPACK_NSIS_DISPLAY_NAME}.lnk' '$INSTDIR\\bin\\${PROJECT_NAME}.exe'")

    # Tell NSIS to create a directory for your application under the install root
    set(CPACK_NSIS_PACKAGE_DIR_ICON "${CMAKE_SOURCE_DIR}/assets/EnhancingDeformationAnalysisUI.icns") # Optional: provide an icon for the installer itself
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/${PROJECT_NAME}.exe") # Icon for Add/Remove Programs

    # Include the uninstaller
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL "ON") # Recommended

    set(VCPKG_TARGET_ARCHITECTURE x64)
    set(VCPKG_CRT_LINKAGE dynamic)
    set(VCPKG_LIBRARY_LINKAGE dynamic)
    set(VCPKG_PLATFORM_TOOLSET "v140")
    set(VCPKG_DEP_INFO_OVERRIDE_VARS "v140")

    # You can also add custom NSIS script sections if needed for very complex installs
    # set(CPACK_NSIS_CUSTOM_INSTALL_COMMANDS ...)

    # For more complex file installations with NSIS, you might need to specify install groups
    # or directly manipulate NSIS scripts via CPACK_NSIS_EXTRA_INSTALL_COMMANDS.
    # However, for a simple bin + assets structure, component installation should work.

    # If components are not working as expected, an alternative is to explicitly list files/dirs
    # but this is less maintainable than component-based installation.
    # Example (less recommended if components can work):
    # set(CPACK_INSTALL_FILES
    #   "bin/${PROJECT_NAME}.exe"   # Path relative to CMAKE_INSTALL_PREFIX
    #   # You'd have to list all DLLs here, which is not ideal.
    # )
    # set(CPACK_INSTALL_DIRECTORIES
    #   "bin"     # Path relative to CMAKE_INSTALL_PREFIX
    #   "assets"  # Path relative to CMAKE_INSTALL_PREFIX
    # )
    # This CPACK_INSTALL_FILES/DIRECTORIES is more for basic CPack generators, NSIS
    # often works better with components or more specific NSIS commands.
    # Stick with CPACK_COMPONENTS_ALL for now.

  elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    # For a .app bundle, ensure MACOSX_BUNDLE is set on the target in src/CMakeLists.txt
    # And install the target using BUNDLE DESTINATION.
    # set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/assets/img/dmg-background.png") # Example background
  else() # Linux
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Enhancing Deformation Analysis Capstone Team")
    set(CPACK_RPM_PACKAGE_RELEASE "1")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Science")
  endif()

  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
  set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};ALL;/")

  include(CPack)
else()
  message(STATUS "CPack is disabled for APP_BUILD_MODE: ${APP_BUILD_MODE}")
endif()
