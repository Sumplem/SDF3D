# SDF3D

SDF3D is a native desktop SDF modeling prototype. Geometry is authored as a node graph, compiled into GLSL, and rendered with GPU raymarching in an ImGui editor.

## Current Status

- C++17 CMake project
- GLFW window, OpenGL 4.6 core through GLAD, Dear ImGui docking UI
- Runtime `assets/` copy next to the executable
- Node editor with primitives, transforms, booleans, materials, output node, selection, duplicate/delete, drag links, auto-insert preview, and incomplete-node bypass preview
- Active graph navigation for root graph and group subgraphs
- Node groups: `Ctrl+G`, Tab enter, Shift+Tab exit, breadcrumbs, JSON `definitions[]`, one `sdf` output
- GPU raymarch viewport with orbit camera, zoom, gizmos, selection highlight, hover highlight, and GPU node-id picking through an integer FBO attachment
- Translate, Rotate, and Scale gizmos with CPU-side hit testing and runtime node parameter updates
- `GlslEmitter` split by concern: dispatch, primitives, booleans, domain, materials, scene assembly, math, formatting
- Deferred material evaluation with `MaterialRegistry`, `SolidMaterial`, `CheckerMaterial`, and `MaterialOverride`
- Progressive path tracing mode with stochastic GI, direct light shadows, roughness-aware sampling, and shader-local denoise
- Clean JSON save/load through `JsonGraphSerializer`; sockets, selection, and legacy material blobs are not serialized
- Shared graph validity in `GraphSystemValidity.cpp`; compiler and UI consume the same validity/bypass queries
- Test coverage split across graph, compiler, renderer support, serializer, material, selection, and system targets

## Requirements

- CMake 3.21 or newer
- C++17 compiler
- Windows primary path: Visual Studio 2022 Build Tools or newer
- Python 3 for GLAD source generation

GLAD generation needs Python packages from its fetched requirements file. If configure/build fails with `No module named 'jinja2'`, install them after configure:

```powershell
python -m pip install -r build\_deps\glad-src\requirements.txt
```

## Build

From the repository root:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Build only the app:

```powershell
cmake --build build --config Debug --target sdf3d
```

Executable:

```text
build\Debug\sdf3d.exe
```

Runtime assets:

```text
build\Debug\assets
```

## Run

```powershell
.\build\Debug\sdf3d.exe
```

Viewport controls:

- Right mouse drag: orbit camera
- Mouse wheel: zoom
- Left click geometry: select node using GPU node-id picking
- Hover geometry: preview highlight when viewport hover toggle is enabled

Node editor controls:

- Add menu: create primitives, transforms, booleans, materials, groups, and output wiring
- Drag links between sockets to wire graph nodes
- Shift-click or drag rectangle: multi-select
- `Ctrl+D`: duplicate selection
- Delete: delete selected nodes
- `F`: frame selection
- `P`: preview selected node
- `Ctrl+G`: group selected nodes
- Tab: enter selected group
- Shift+Tab: exit group
- Breadcrumb click: jump to group scope

## Tests

Run one target:

```powershell
cmake --build build --config Debug --target sdf3d_graph_system_tests
.\build\Debug\sdf3d_graph_system_tests.exe
```

Run all test executables after building:

```powershell
Get-ChildItem build\Debug -Filter sdf3d*_tests.exe | ForEach-Object { & $_.FullName }
```

Current test targets:

- `sdf3d_tests`
- `sdf3d_graph_tests`
- `sdf3d_graph_compiler_tests`
- `sdf3d_uniform_uploader_tests`
- `sdf3d_fbo_renderer_tests`
- `sdf3d_path_trace_accumulation_tests`
- `sdf3d_shader_manager_tests`
- `sdf3d_diagnostics_tests`
- `sdf3d_event_bus_tests`
- `sdf3d_selection_tests`
- `sdf3d_graph_system_tests`
- `sdf3d_graph_serializer_tests`
- `sdf3d_glsl_emitter_tests`
- `sdf3d_compiler_system_tests`
- `sdf3d_material_system_tests`

## Project Layout

```text
include/sdf3d/          Headers and public contracts
src/app/                Application wiring
src/core/               Event bus and resource helpers
src/renderer/           OpenGL renderer, shaders, FBOs, uniforms
src/scene/              Graph, node metadata, graph lowering
src/systems/            Graph logic, compiler, serializer, materials, diagnostics
src/ui/                 ImGui editor panels and node editor
assets/shaders/         Runtime GLSL shader assets
tests/                  Dependency-light executable tests
cmake/                  FetchContent dependency setup
build/                  Local generated build output
```

## Architecture Notes

- Components stay data-only.
- Systems own logic and do not call OpenGL.
- Renderer owns OpenGL and does not include scene graph headers.
- App wires systems, renderer, UI, and events.
- Runtime shaders are asset files under `assets/shaders`.
- `raymarch_edit.frag` is the editor shader; `raymarch.frag` is scene/export preview; `raymarch_pathtrace.frag` is progressive path tracing.
- GPU picking is editor-only: `raymarch_edit.frag` writes node ids to a `GL_R32I` FBO attachment, and the viewport reads one pixel.
- `raymarch.frag` and `raymarch_pathtrace.frag` stay free of picking outputs.

## Deferred Work

- Node group exposed parameters
- Group Make Unique
- Group library or cross-scene sharing
- More procedural material source nodes
- Animation/keyframes
