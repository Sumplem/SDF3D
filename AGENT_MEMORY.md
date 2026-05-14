# Agent Memory

## Active Instructions

- Use caveman mode: terse, technical, low filler.
- Follow SDF3D `AGENTS.md` workflow: plan first, one file or one logical unit, review gate after each file.
- Do not advance beyond approved file.
- Do not modify unapproved files.

## Current State

- M3/M4-ish scene compiler and UI already exist.
- Build currently passes with `cmake --build build --config Debug`.
- Tests currently pass with `build\Debug\sdf3d_tests.exe`.
- GUI smoke test passed: empty-scene `sdf3d.exe` stayed running for 3 seconds before test stop.

## Recent Approved Edits

- `src/ui/UI.cpp`
  - Added Properties panel material controls:
    - `Albedo`
    - `Roughness`
    - `Metallic`
    - `Emission`
  - Material edits call `markSceneDirty()`.
  - Add menu primitive creation now creates `SdfGraph` nodes.
  - Scene panel lists graph nodes with select/output/delete controls.
  - Properties panel edits selected graph node payload when graph selection exists.
  - Scene panel has manual graph link controls: From, To, Input, Link, Unlink Input, Current Links.
  - Add menu transform/boolean entries now create graph nodes with default parameters.
  - Graph labels now include boolean/smooth node type names and stable node IDs.
  - Link input uses preset socket combo: `child`, `base`, `cutter`, `left`, `right`, `custom`.
  - New graph node becomes output; successful link sets target node as output.
  - Transform/boolean Add menu shortcuts auto-link previously selected graph node into the new operation.
  - Selected boolean graph node shows `Add Operand`, creating a 0.5 radius sphere linked to `right`/`cutter`.
  - Legacy tree controls are no longer displayed in Scene panel, including startup empty graph.
  - Removed unused legacy tree helper definitions from implementation.
  - Added `Clear Graph` action using public `SdfGraph::deleteNode()`.
  - Scene panel shows typed input/output sockets under each graph node.
  - Link controls connect a selected output socket to a selected input socket.
  - Scene panel now uses a canvas-style node editor:
    - draggable node cards
    - visible socket pins
    - Bezier link wires
    - click output pin then input pin to connect
    - selected output pin highlighted
    - per-node Set Output/Delete buttons

- `include/sdf3d/ui/UI.h`
  - Removed unused `ScenePanel` member/include and stale primitive counter.
  - Removed unused legacy tree helper declarations.

- `include/sdf3d/scene/SdfCompiler.h`
  - Added `SdfCompiledMaterial`.
  - Added `SdfCompileResult::materials`.
  - Added `compile(const SdfGraph&)` declaration.

- `src/scene/SdfCompiler.cpp`
  - Added material collection during compile.
  - Primitive leaves append their `SdfMaterial` and emit material ID.
  - Generated GLSL now includes:
    - `vec2 sceneSDFWithMaterial(vec3 p)`
    - `float sceneSDF(vec3 p)` wrapper for current shader compatibility.
  - Boolean/domain ops preserve material IDs.
  - Smooth ops currently keep nearer source material; no material blending yet.
  - Implemented `compile(const SdfGraph&)` by building a deterministic temporary tree from graph output links.
  - Graph compile detects cycles and missing node references.
  - Graph compile orders known sockets: `child`, `left`/`base`, `right`/`cutter`, then fallback lexical.

- `assets/shaders/raymarch.frag`
  - Raymarch hit path reads material ID from `sceneSDFWithMaterial()`.
  - Uses material albedo/emission uniforms with fallback color.
  - Keeps `sceneSDF()` wrapper for normal estimation.

- `include/sdf3d/renderer/Renderer.h`
  - Added `setMaterials(std::vector<SdfCompiledMaterial>)`.
  - Added renderer material storage.

- `src/renderer/Renderer.cpp`
  - Implemented `setMaterials()`.
  - Uploads material uniform arrays before draw, capped at 64.

- `src/app/App.cpp`
  - Calls `m_renderer.setMaterials(sceneGlsl.materials)` after initial compile and dirty recompiles.
  - Added `compileScene()` helper: compile graph when graph output exists, else tree fallback.

- `tests/sdf_node_tests.cpp`
  - Added material metadata tests for count, order, and preserved edited values.
  - Added `SdfGraph` tests for create/select/output, link replacement, delete cleanup, and invalid ID validation.
  - Added graph socket/default typed link validation tests.
  - Added graph compiler tests for empty graph, primitive payload/material, linked transform, and cycle error.
  - Added graph compiler socket ordering test for subtract `base`/`cutter`.

- `src/scene/SceneGraph.cpp`
  - Constructor restored to empty scene.
  - Implemented `graph()` accessors.

- `include/sdf3d/scene/SceneGraph.h`
  - Added `SdfGraph m_graph` ownership and `graph()` accessors for migration.
  - Existing tree API preserved.

- `src/scene/SdfCompiler.cpp`
  - Empty scene fallback now emits `sceneSDFWithMaterial()` plus `sceneSDF()`.
  - This keeps shader valid without default geometry.

- `include/sdf3d/scene/SdfGraph.h`
  - Added first graph data-model declarations.
  - Defines stable graph node IDs, directed links, graph nodes with `SdfNode` payload, output node, selection, and read APIs.
  - Added Blender-like socket metadata foundation:
    - `SdfSocketType`
    - `SdfSocketDirection`
    - `SdfGraphSocket`
    - `SdfGraphLink::fromSocket`
    - typed `inputs`/`outputs` on graph nodes.
  - Kept compatibility constructors/API for current string-socket link path.

- `src/scene/SdfGraph.cpp`
  - Implemented create/delete/link/unlink/select/output/read APIs.
  - Added to CMake and now compiles.
  - Populates default typed sockets per node type.
  - Validates typed links by output/input socket existence and matching socket type.

- `CMakeLists.txt`
  - Added `src/scene/SdfGraph.cpp` to `sdf3d` and `sdf3d_tests`.

## Next Proposed Step

- Decide next M4/M5 increment.
- Likely candidates:
  - Add a dedicated Output node type/semantic instead of output flag on any node.
  - Improve material shader usage for roughness/metallic.
  - Add shader/runtime validation path.
  - Add UI polish for material parameters.

## Known Caveat

- Shader currently uses albedo/emission only.
- Roughness/metallic uniforms upload but shader does not use them yet.
- Need plan/approval before touching feature code.
