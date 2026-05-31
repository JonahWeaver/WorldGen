# WorldGen Fantasy Map Generator

A small C++ starter project using Dear ImGui for a fantasy map generator UI.

## Features

- ImGui side panel with map generation settings
- `Generate` button to produce tectonic plates and fault lines
- Real-time preview of the generated plate map

## Build Instructions

1. Open a terminal in the project folder.
2. Create a build directory:
   ```powershell
   mkdir build
   cd build
   ```
3. Run CMake and build:
   ```powershell
   cmake ..
   cmake --build . --config Release
   ```
4. Run the executable:
   ```powershell
   .\Release\WorldGen.exe
   ```

## Notes

- This starter uses CMake `FetchContent` to download GLFW, GLAD, and Dear ImGui automatically.
- The map preview shows colored plates and dark fault lines.
