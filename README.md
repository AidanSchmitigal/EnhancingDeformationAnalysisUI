# EnhancingDeformationAnalysisUI
- To clone this repo, do `git clone --recursive git@github.com:OSU-Enhancing-Deformation-Analysis/EnhancingDeformationAnalysisUI`
Don't miss the recursive part of that command!
- If you missed that part, you need to initialize the submodules with the command `git submodule update --init`

## Building
### Prerequisites
- CMake 3.5 or higher
- OpenCV ~4.0.0 or higher with the OpenCV_DIR environment variable set to the path of the OpenCVConfig.cmake file
- Python3.8 or higher (for compatibility with pybind11) with python development installed
- [Tensorflow C API](https://www.tensorflow.org/install/lang_c) installed on system path (findable by CMake)
- On windows: CUDA 11 & cuDNN 8.x.x installed to PATH
- On linux: CUDA 12.x & cuDNN 9.x.x installed to PATH

### Compilation
#### Linux (maybe MacOS?)
- To build (after installing prerequisites), follow these commands: 
- `cd EnhancingDeformationAnalysisUI && mkdir build`
- `cd build && cmake ..`
- `make -j4` (or however many cores you want to use)
- This also seems to be required for Cuda support on Linux: `export XLA_FLAGS=--xla_gpu_cuda_data_dir=/usr/lib/cuda`

#### Windows
- Use visual studio with cmake installed
- Open the EnhancingDeformationAnalysisUI folder with visual studio
- It should generate a .sln file using CMake, autosetup to run.
