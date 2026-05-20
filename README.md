# HwaSimIR

HwaSimIR is a C++ infrared simulation and graphics project collection. The repository contains Qt, Visual Studio, CMake, Panda3D, OpenCV, and Eigen based examples or applications.

## Project Layout

### DataDrivenTestQT

- Project file: `DataDrivenTestQT/DataDrivenTestQT.pro`
- Type: Qt Widgets qmake application
- Entry point: `DataDrivenTestQT/main.cpp`
- Main UI files: `mainwindow.cpp`, `mainwindow.h`, `mainwindow.ui`
- Qt modules: Core, GUI, Network, Widgets

Recommended startup:

1. Open `DataDrivenTestQT/DataDrivenTestQT.pro` in Qt Creator.
2. Select a Qt 5.12.12 MinGW kit.
3. Configure, build, and run from Qt Creator.

Command-line builds can also use `qmake` and `mingw32-make` when the Qt toolchain is available in `PATH`.

### MaterialTest

- Solution file: `MaterialTest/MaterialTest.sln`
- Type: Visual Studio C++ console application
- Toolset: Visual Studio v140
- Main source: `MaterialTest/MaterialTest/main.cpp`
- Graphics dependency: Panda3D

Recommended startup:

1. Configure Panda3D include and library paths, for example through a `PANDA3D_DIR` environment variable or local Visual Studio project settings.
2. Open `MaterialTest/MaterialTest.sln` in Visual Studio.
3. Select the desired Win32/x64 and Debug/Release configuration.
4. Build and run from Visual Studio.

### ConsoleApplication1_LLA

- Solution file: `ConsoleApplication1_LLA/ConsoleApplication1.sln`
- CMake file: `ConsoleApplication1_LLA/ConsoleApplication1/CMakeLists.txt`
- Type: Visual Studio and CMake C++ simulation application
- Main source: `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.cpp`
- Dependencies: Panda3D, OpenCV, Eigen

Recommended Visual Studio startup:

1. Configure Panda3D and OpenCV include/library paths through environment variables or local Visual Studio project settings.
2. Open `ConsoleApplication1_LLA/ConsoleApplication1.sln`.
3. Select the desired Win32/x64 and Debug/Release configuration.
4. Build and run from Visual Studio.

Recommended CMake startup:

1. Install or build Panda3D for the target platform.
2. Install Eigen and make it discoverable by the compiler.
3. Adjust local CMake configuration or environment variables for Panda3D and other native dependencies.
4. Configure and build with CMake using a build directory outside the source tree.

## Dependencies

Prepare the following environment before building all projects:

- Qt 5.12.12 with MinGW for `DataDrivenTestQT`
- Visual Studio with v140 platform toolset
- Windows SDK matching the Visual Studio installation
- Panda3D C++ SDK or locally built Panda3D libraries
- OpenCV development package for `ConsoleApplication1_LLA`
- Eigen headers for `ConsoleApplication1_LLA`

Do not rely on another developer's absolute local paths. Configure dependencies with environment variables, Visual Studio property pages, CMake cache entries, or machine-local toolchain settings.

## Runtime Assets

The repository includes simulation data, configuration files, models, textures, shaders, and database files under directories such as `materials`, `temperatures`, `transmittance`, and project `Bin` asset folders. These files may be required at runtime.

Before publishing to GitHub, review large binary assets and decide whether they should remain in Git, move to release artifacts, or be tracked with Git LFS.

## GitHub Preparation Checklist

- Confirm which projects should be part of the public repository.
- Build each project locally after dependency paths are configured.
- Review `git status --short` before staging files.
- Check that build outputs, IDE state, `.env` files, and private keys are ignored.
- Review large assets such as textures, models, PDFs, and databases.
- Add a license file if the repository will be shared publicly.
- Document any required runtime data paths or environment variables.
