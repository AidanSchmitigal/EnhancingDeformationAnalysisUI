# EnhancingDeformationAnalysisUI

A tool for analyzing SEM imagery of materials undergoing deformation, with AI-powered denoising and crack detection.

## Installation

To clone this repo:
```bash
git clone --recursive git@github.com:OSU-Enhancing-Deformation-Analysis/EnhancingDeformationAnalysisUI
```
Don't miss the `--recursive` part of that command to get all required submodules!

If you missed that part, initialize the submodules with:
```bash
git submodule update --init
```

This program uses [tk_r_em](https://github.com/Ivanlh20/tk_r_em)'s models for denoising.

## Building

### Prerequisites
- CMake 3.5 or higher
- OpenCV ~4.0.0 or higher with the OpenCV_DIR environment variable set to the path of the OpenCVConfig.cmake file
- [Tensorflow C API](https://www.tensorflow.org/install/lang_c) installed on system path (findable by CMake)
- On Windows: CUDA 11 & cuDNN 8.x.x installed to PATH
- On Linux: CUDA 12.x & cuDNN 9.x.x installed to PATH, Zenity for file browsing operations

### Compilation

#### Linux (maybe macOS?)
```bash
cd EnhancingDeformationAnalysisUI && mkdir build
cd build && cmake ..
make -j4  # or however many cores you want to use
```

For CUDA support on Linux:
```bash
export XLA_FLAGS=--xla_gpu_cuda_data_dir=/usr/lib/cuda
```

#### Windows
1. Use the setup script `setup.py` to set up the project and install OpenCV and TensorFlow
2. Make sure to set the environment variables as instructed
3. Open the folder using Visual Studio (assuming Desktop C++ package installed)
4. Select the correct startup item from the dropdown: `EnhancingDeformationAnalysisUI.exe`
5. Use F5 to build and run

## Using the Application

### GUI Mode

Launch the application without any command-line arguments to use the GUI:

1. **Loading Images**: 
   - Click "Select Folder" to choose a directory containing TIFF images
   - Each image set opens in its own tab

2. **Preprocessing**:
   - Crop images to remove unneeded parts
   - Stabilize image sequence to reduce movement
   - Remove/filter specific frames
   - Apply denoising (blur or AI-powered models)
   - Detect cracks with adjustable parameters

3. **Image Analysis**:
   - View histograms for each image
   - Export analysis data as CSV

4. **Feature Tracking**:
   - Track points manually by selecting features
   - Track automatically using detected cracks
   - Measure deformation over time

### Command Line Interface

For batch processing, use the CLI mode:

```bash
./EnhancingDeformationAnalysisUI --folder <path> [--crop <pixels>] [--denoise <blur/sfr_hrsem/...>] [--analyze <output.csv>] [--calculate-widths <widths.csv>] [--output <path>]
```

Options:
- `--folder <path>`: Directory containing TIFF images (required)
- `--crop <pixels>`: Remove specified number of pixels from image bottom
- `--denoise <filter>`: Apply denoising filter (options: blur, sfr_hrsem, sfr_lrsem, etc.)
- `--analyze <output.csv>`: Generate image analysis statistics
- `--calculate-widths <widths.csv>`: Calculate and export crack width measurements
- `--output <path>`: Save processed images to specified directory

## Features
- Loading folders of .tif/tiff images
- Preprocessing
    - Cropping images
    - Stabilization
    - Frame selection and removal
    - Denoising with blur or AI models
    - Crack detection
- Image Analysis with histograms (exportable)
- Feature tracking via user-selected points or automatic crack detection
- Deformation analysis
    - Strain map generation with AI model

## Credits
- [tk_r_em](https://github.com/Ivanlh20/tk_r_em) for the AI models used for denoising
