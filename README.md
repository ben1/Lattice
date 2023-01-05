# Introduction

This is a small C/C++ app that brings together several opensource libraries for use as a starting point for prototyping or making simple games.

Libraries used are:
* SDL for graphics/audio
* Dear ImGui for UI
* FastBuild for a build system as well as for many low level utility functions.
* FreeType used for font rendering

OpenGL3 is the default rendering backend, but can be changed to anything SDL supports.

# Building

Currently Visual Studio on Windows is supported, but GCC/Clang/OSX/Linux would be easy to add.

## Visual Studio

**Note**: You may need to edit the files in Build/VisualStudio to set the VS version you have installed.

1. To generate the solution, run:
> External\FastBuild\1.08\Bin\win32-x64\FBuild.exe solution
2. Open the solution in 'tmp\VisualStudio' and hit F5.

If new files are added to the project outside of Visual Studio, the solution can be regenerated by building the Build\UpdateSolution project.

# License

Each library inside the 'External' folder has its own license which must be adhered to.

The build files were based on build files from FastBuild, and the Main.cpp was copied and modified from the example in imgui.

Other code is covered by the 'LICENSE.TXT' in this folder.
