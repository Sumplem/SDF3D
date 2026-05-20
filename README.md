# SDF3D (Vibe Code Project)

SDF3D is a native desktop 3D modeling application prototype where geometry is represented as Signed Distance Functions instead of polygon meshes. The current codebase has the M1 skeleton and M2 first raymarch working, with the M3 scene graph/compiler foundation in progress.

## Current Status

-   CMake project using C++17
-   GLFW window and OpenGL 4.6 core context
-   Dear ImGui docking layout with Scene, Viewport, and Properties panels
-   FetchContent dependencies for GLFW, Dear ImGui, GLM, and GLAD
-   Runtime `assets/` copy next to the executable
-   Cross-platform executable directory helper
-   ResourceManager singleton with fallback magenta shader and checkerboard texture
-   Raymarched sphere rendered into an ImGui viewport texture
-   Basic orbit camera and mouse wheel zoom
-   SDF node AST, empty scene graph support, and deterministic GLSL compiler
-   Minimal dependency-free compiler tests through `sdf3d_tests`

## Requirements

-   CMake 3.21 or newer
-   C++17 compiler
-   Windows: Visual Studio 2022 Build Tools or newer
-   Python 3 for GLAD source generation

GLAD's generator requires the Python packages listed in its fetched `requirements.txt`. If the build fails with `No module named 'jinja2'`, install it after configure:

```powershell
python -m pip install -r build\_deps\glad-src\requirements.txt
```

## Build

From the repository root:

```powershell
cmake -S . -B build
cmake --build build --config Debug --target sdf3d
```

The executable is generated at:

```text
build\Debug\sdf3d.exe
```

Runtime assets are copied to:

```text
build\Debug\assets
```

## Run

```powershell
.\build\Debug\sdf3d.exe
```

The application opens a dockable editor window with an empty scene. Use the `Add` menu to create geometry.

Viewport controls:

-   Right mouse drag: orbit camera
-   Mouse wheel: zoom

Scene editing:

-   `Add > Sphere/Box/Cylinder/Torus/Plane`: create primitives
-   `Add > Transform`: wrap the selected node in Translate, Rotate, or Scale
-   `Add > Boolean`: combine the selected node with a temporary default operand
-   Scene panel: select, duplicate, delete, and reorder nodes
-   Properties panel: edit selected node name and float parameters

## Test

```powershell
cmake --build build --config Debug --target sdf3d_tests
.\build\Debug\sdf3d_tests.exe
```

## Project Layout

```text
include/sdf3d/          Public and internal headers
src/                    C++ implementation files
assets/                 Runtime assets copied beside the executable
cmake/                  Dependency setup
build/                  Local generated build output
```

## Development Notes

-   Internal includes use the full namespaced path, for example `#include "sdf3d/app/App.h"`.
-   Dependencies are managed through CMake FetchContent.
-   Do not hardcode absolute asset paths; resolve runtime assets relative to the executable.
-   Scene and Properties panels are still placeholders.
-   M3/M4 currently compile the editable scene into GLSL and reload the viewport shader after scene edits.

## Known UX Debt

-   The Scene panel tree is a temporary editor for validating scene graph operations.
-   Move Up / Move Down controls should be revisited when node graph editing exists.
-   Boolean menu actions currently auto-create a translated sphere operand so the operation is immediately visible.
-   A node graph editor should eventually replace or supplement the tree for boolean/domain composition.
