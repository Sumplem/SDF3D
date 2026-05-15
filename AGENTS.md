# Agent Memory

## Active Instructions

- Use caveman mode: terse, technical, low filler.
- Follow SDF3D `AGENTS.md` workflow: plan first, one file or one logical unit, review gate after each file.
- Do not advance beyond approved file.
- Do not modify unapproved files.

## Current State

- Use the current repo root / process working directory. Do not assume a fixed absolute path; this repo may live at different paths on different machines.
- Local Windows paths seen in prior runs are historical only and not project requirements.
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
    - drag from output pin and release over compatible input pin to connect
    - active drag wire and compatible input pin highlighted
    - per-node Delete button only; Output node delete is disabled
  - UI split refactor completed:
    - `UI.cpp` is a thin compositor.
    - `AddMenu.h/.cpp` owns Add menu actions.
    - `NodeEditor.h/.cpp` owns graph canvas, node drag, pin hit-test, Bezier wires, and drag-to-connect.
    - `SceneOutliner.h/.cpp` owns graph controls, selection, delete, manual link controls, and Add Operand.
    - `PropertiesPanel.h/.cpp` owns node name, material, and parameter editing.
    - Removed obsolete `ScenePanel.h/.cpp`.

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
  - Added graph Output node support:
    - `SdfNodeType::Output`
    - input socket `surface`
    - no output sockets
    - active Output node stays render target; adding/linking regular nodes should not steal graph output.

- `src/scene/SdfGraph.cpp`
  - Implemented create/delete/link/unlink/select/output/read APIs.
  - Added to CMake and now compiles.
  - Populates default typed sockets per node type.
  - Validates typed links by output/input socket existence and matching socket type.
  - Output nodes expose only `surface` input and no `sdf` output.

- `src/scene/SdfCompiler.cpp`
  - Graph compiler now treats an active Output node as render target marker.
  - If Output node is linked, compiler follows `Output.surface` to the real SDF root.
  - If Output node is unlinked, compiler returns safe no-hit GLSL and an error.
  - Incomplete graph nodes must never emit invalid GLSL:
    - no valid child returns material-aware no-hit `vec2(1e6, 0.0)`
    - single-input Union/SmoothUnion/Intersect/SmoothIntersect bypasses to the child
    - Subtract/SmoothSubtract with only base input bypasses to base and warns
    - missing transform child returns no-hit and warns
  - Graph lowering was split out into `SdfGraphCompiler.h/.cpp`.
  - `compileNode()` was split into private family helpers:
    - `compilePrimitiveNode`
    - `compileBooleanNode`
    - `compileDomainNode`

- `src/ui/AddMenu.cpp`
  - Uses `SdfNodeDefinition` metadata for primitive/boolean/transform menu entries.
  - `Add > Output` is removed.
  - New primitives auto-link into selected node's first free SDF input when possible.

- `src/ui/NodeEditor.cpp`
  - Link UX now drag-to-connect:
    - drag from output pin
    - live Bezier wire follows cursor
    - compatible input pins highlight
    - release on compatible input creates graph link
    - release elsewhere cancels
  - Link removal UX:
    - right-click a Bezier link to remove it
    - drag an occupied input link away and release on empty canvas to remove it
    - drag an occupied input link to another compatible input to reconnect it
  - Output node behavior:
    - `SdfGraph` now creates one default Output node on construction
    - Output node cannot be deleted
    - `Add > Output` is removed
    - first added primitive auto-links into `Output.surface`
    - graph output target must remain an Output node; regular nodes cannot become render output markers
  - Preview shortcut:
    - press `P` while node editor is focused to rewire selected node `sdf` into `Output.surface`
    - shortcut is no-op for Output node, no selection, or nodes without `sdf` output
  - Auto-wire / virtual visual behavior:
    - dragging an unconnected compatible node over a wire shows a virtual insert preview
    - preview dims original wire and draws yellow `A -> dragged -> B` wires
    - real graph links are changed only on mouse release
    - incomplete nodes get orange border and missing SDF input pin rings
    - incomplete boolean bypass cases draw orange virtual bypass wires with no graph mutation
    - visual bypass mirrors compiler behavior for single-input Union/SmoothUnion/Intersect/SmoothIntersect and base-only Subtract/SmoothSubtract

- `include/sdf3d/scene/SdfNodeDefinition.h` / `src/scene/SdfNodeDefinition.cpp`
  - Central metadata table now owns Phase 1 node display names, categories, sockets, default params, and UI drag hints.
  - `SdfGraph` uses metadata to create default sockets/payload.
  - `PropertiesPanel` uses metadata parameter order and drag min/max/step.

- `assets/shaders/raymarch.frag`
  - Uses albedo, emission, roughness, and metallic.
  - Roughness controls Blinn-Phong shininess/specular strength.
  - Metallic tints specular toward albedo and reduces diffuse.

- `src/app/App.cpp` / `src/renderer/Renderer.cpp` / `src/ui/UI.cpp`
  - Runtime diagnostics flow added.
  - Renderer stores latest shader compile/link/reload error.
  - Dirty shader reload failure keeps previous shader program alive.
  - Diagnostics panel shows compiler/renderer runtime errors.

- `CMakeLists.txt`
  - Added `src/scene/SdfGraph.cpp` to `sdf3d` and `sdf3d_tests`.
  - Added UI split sources: `AddMenu.cpp`, `NodeEditor.cpp`, `SceneOutliner.cpp`, `PropertiesPanel.cpp`.
  - Added `SdfGraphCompiler.cpp` and `SdfNodeDefinition.cpp`.

## Next Proposed Step

- Decide next M4/M5 increment.
- Likely candidates:
  - Node editor pan/zoom.
  - Keyboard delete / duplicate / frame selected node.
  - Save/load graph schema.
  - Split compiler family helpers into separate files if compiler grows again.

## Known Caveat

- Need plan/approval before touching feature code.
- GUI smoke should be rerun after major NodeEditor visual work.
