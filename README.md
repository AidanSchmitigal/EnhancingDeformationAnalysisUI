# EnhancingDeformationAnalysisUI
- To clone this repo, do `git clone --recursive git@github.com:OSU-Enhancing-Deformation-Analysis/EnhancingDeformationAnalysisUI`
Don't miss the recursive part of that command! If you missed that part, you need to initialize the submodules with the command `git submodule update --init`

## Building
### Prerequisites
- CMake 3.5 or higher
- OpenCV 4.5.1 or higher with the OpenCV_DIR environment variable set to the path to the OpenCVConfig.cmake file
- Python3.8 or higher (for compatibility with pybind11)

- To build, follow these commands: 
- `cd EnhancingDeformationAnalysisUI && mkdir build`
- `cd build && cmake ..`
- `make -j4` (or however many cores you want to use)