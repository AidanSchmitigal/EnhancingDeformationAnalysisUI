# EnhancingDeformationAnalysisUI
- To clone this repo, do `git clone --recursive git@github.com:OSU-Enhancing-Deformation-Analysis/EnhancingDeformationAnalysisUI`
Don't miss the recursive part of that command!
- If you missed that part, you need to initialize the submodules with the command `git submodule update --init`
- This program uses [tk_r_em](https://github.com/Ivanlh20/tk_r_em)'s models for denoising.

## Building
### Prerequisites
- CMake 3.5 or higher
- OpenCV ~4.0.0 or higher with the OpenCV_DIR environment variable set to the path of the OpenCVConfig.cmake file
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
- Use the setup script `setup.py` to setup the project and install opencv and tensorflow. Make sure to set the environment variables.
- Open the folder using Visual Studio (assuming Desktop C++ package installed). This should automatically configure the project using CMake.
- Select the correct startup item from the dropdown with the green arrow, EnhancingDeformationAnalysisUI.exe
- Use F5 to build and run.

## Features
- Loading folders of .tif/tiff images
- Preprocessing
    - Cropping images
    - Stabilization
    - Frame selection and removal
    - Denoising with blur or [tk_r_em](https://github.com/Ivanlh20/tk_r_em) AI models
    - Crack detection
- Image Analysis with histograms (exportable)
- Feature tracking either via user selected points or automatic using the crack detection algorithm
