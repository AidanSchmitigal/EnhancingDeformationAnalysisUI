# EnhancingDeformationAnalysisUI
- To clone this repo, do `git --recursive clone git@github.com:OSU-Enhancing-Deformation-Analysis/EnhancingDeformationAnalysisUI`
Don't miss the recursive part of that command! If you missed that part, you need to initialize the submodules with the command `git submodule update --init`

## Building
- To build, follow these commands: `cd EnhancingDeformationAnalysisUI && mkdir build`
`cmake -S . -B build/ && make -j4`
