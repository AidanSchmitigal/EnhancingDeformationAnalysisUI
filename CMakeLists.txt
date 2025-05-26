cmake_minimum_required(VERSION 3.16)
project(EnhancingDeformationAnalysisUI VERSION 0.2 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

option(UI_INCLUDE_TENSORFLOW "Include TensorFlow in the project" ON)
option(UI_INCLUDE_PYTORCH "Include PyTorch in the project" OFF)
option(UI_PROFILE "Build the project in profile mode" OFF)

add_subdirectory(src)
