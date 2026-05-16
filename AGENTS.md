# Agent Memory

## Active Instructions

- Use caveman mode: terse, technical, low filler.
- Follow SDF3D `AGENTS.md` workflow: plan first, one file or one logical unit, review gate after each file.
- Do not advance beyond approved file.
- Do not modify unapproved files.
- Future multi-file UI modules should be physically grouped in subfolders, e.g. NodeEditor internals live under `src/ui/node_editor/` with internal headers under `include/sdf3d/ui/node_editor/`.

## Current State

- Use the current repo root / process working directory. Do not assume a fixed absolute path; this repo may live at different paths on different machines.
- Local Windows paths seen in prior runs are historical only and not project requirements.
- M3/M4-ish scene compiler and UI already exist.
- Build currently passes with `cmake --build build --config Debug`.
- Full test suite currently passes; see latest Material Separation entry below for exact executables.
- GUI smoke test passed: debug `sdf3d.exe` stayed running for 3 seconds before test stop.

## Current Material / Compiler Model

- `SdfNodeType::MaterialOverride` is the explicit editable material node.
- `SdfNode` still carries `SdfMaterial` payload storage for compatibility/migration, but UI and compiler material assignment must treat it as active only on `MaterialOverride`.
- Properties panel and inline node controls show material controls only for `MaterialOverride`.
- `CompilerSystem::compile(graph)` lowers graph to a temporary tree, then `GlslEmitter` emits geometry helpers plus deferred material GLSL.
- Primitive nodes emit geometry-only `sdf_node_<id>` helpers and use default material id `0`.
- `MaterialOverride` nodes pass child SDF distance through in geometry helpers and contribute material ids only in deferred `sceneMaterial`.
- Generated GLSL exposes `float sceneSDF(vec3 p)` plus `SdfMaterialSample sceneMaterial(vec3 p)`.
- Boolean/domain material behavior:
  - transforms pass child material through
  - union/intersect choose winner material by distance
  - subtract keeps base material
  - smooth ops blend material samples across the smooth boundary
- `App::recompileScene()` sends GLSL to `Renderer::reloadScene()` and material list to `Renderer::setMaterials()`.
- `UniformUploader` uploads up to 64 materials via uniform arrays.
- Material parameter edits mark material-dirty only and update renderer uniforms without shader reload.
- Temporary migration helper exists: `GraphMigrator::injectMaterialOverrides()` wraps legacy primitive material payloads with `MaterialOverride` nodes.

## Recent Approved Edits

- Deferred material eval Step 1 and emitter split completed:
  - `SdfNode` now carries `stableId` so graph node IDs survive lowering.
  - `SdfGraphCompiler` copies `SdfGraphNode::id` into lowered `SdfNode::stableId`.
  - `GlslEmitter::emitSdfHelpers()` emits geometry-only `float sdf_node_<id>(vec3 p)` helpers in dependency order: leaves first, root last.
  - `MaterialOverride` SDF helper passes through child geometry only.
  - Runtime shader path now uses `sceneSDF()` in the raymarch loop and calls `sceneMaterial()` once after hit confirmation.
  - Material edit path now emits `MaterialDirtyEvent` and calls `Renderer::setMaterials()` without `Renderer::reloadScene()`.
  - `GlslEmitter.cpp` hard-size debt was fixed by splitting internals into `src/systems/glsl_emitter/`:
    - `GlslEmitterFormatting.h/.cpp`
    - `GlslEmitterBooleanExpressions.cpp`
    - `GlslEmitterMaterialExpressions.cpp`
    - `GlslEmitterSdfHelperContext.h`
    - `GlslEmitterSdfGeometry.cpp`
    - `GlslEmitterSdfHelpers.cpp`
  - Build passed:
    - `cmake -S . -B build -DPython_EXECUTABLE=C:\Users\lucgi\AppData\Local\Programs\Python\Python312\python.exe`
    - `cmake --build build --config Debug --target sdf3d sdf3d_tests sdf3d_glsl_emitter_tests sdf3d_compiler_system_tests`
  - Full test set passed:
    - `sdf3d_tests`
    - `sdf3d_compiler_system_tests`
    - `sdf3d_glsl_emitter_tests`
    - `sdf3d_material_system_tests`
    - `sdf3d_selection_tests`
    - `sdf3d_event_bus_tests`
    - `sdf3d_uniform_uploader_tests`
    - `sdf3d_fbo_renderer_tests`
    - `sdf3d_shader_manager_tests`
    - `sdf3d_diagnostics_tests`
  - Environment note: CMake/GLAD regenerate required real Python. Python 3.12 was installed via winget, and GLAD requirements (`jinja2`, `MarkupSafe`) were installed into that interpreter.
  - Next deferred-material step should be Step 2: wire `CompilerSystem` output to use emitted `sdf_node_<id>` helpers and make `sceneSDF(vec3 p)` a thin root-helper wrapper. Plan first; no shader/raymarch edit until approved.

- Material separation completed:
  - `SdfPrimitive` component header remains pure geometry: `type` + `parameters`, no material field.
  - Added `SdfNodeCategory::Material` and metadata entry for `MaterialOverride`.
  - `MaterialOverride` graph node has input `sdf` and output `sdf`.
  - `MaterialSystem::ensureDefaultMaterial()` reserves default material id `0`.
  - `GlslEmitter` no longer appends primitive materials.
  - `GlslEmitter` appends material only at `MaterialOverride` nodes.
  - Primitives without `MaterialOverride` compile to default material id `0`.
  - Add menu now exposes `Materials > Material Override`.
  - `PropertiesPanel` and inline node controls gate material editing to `MaterialOverride`.
  - Node editor right-click primitive context action can `Wrap in Material Override`.
  - Added `include/sdf3d/scene/GraphMigrator.h` and `src/scene/GraphMigrator.cpp`.
  - `GraphMigrator::injectMaterialOverrides()` wraps legacy primitive material payloads and rewires outgoing `sdf` links.
  - Tests updated for default material id and explicit material override behavior.
  - Build passed with `cmake --build build --config Debug`.
  - Full test set passed:
    - `sdf3d_tests`
    - `sdf3d_compiler_system_tests`
    - `sdf3d_glsl_emitter_tests`
    - `sdf3d_material_system_tests`
    - `sdf3d_selection_tests`
    - `sdf3d_event_bus_tests`
    - `sdf3d_uniform_uploader_tests`
    - `sdf3d_fbo_renderer_tests`
    - `sdf3d_shader_manager_tests`
    - `sdf3d_diagnostics_tests`
  - GUI smoke passed: debug `sdf3d.exe` stayed running for 3 seconds.

- `src/ui/UI.cpp`
  - Historical note: material-control bullets below are superseded by Material Separation; current UI gates material controls to `MaterialOverride`.
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
    - multi-select: drag empty canvas rectangle selects multiple nodes; Shift+click toggles nodes in selection
    - shortcuts: `Delete` deletes selected non-output nodes, `Ctrl+D` duplicates selected nodes without links, `F` frames selected nodes, `P` previews primary selected node to Output
  - UI split refactor completed:
    - `UI.cpp` is a thin compositor.
    - `AddMenu.h/.cpp` owns Add menu actions.
    - `NodeEditor.h` plus `src/ui/node_editor/*` own graph canvas, node drag, pin hit-test, Bezier wires, and drag-to-connect.
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
  - Historical note: generated-entry bullets below are superseded by Deferred Material Eval; current GLSL emits `sceneSDF` + `sceneMaterial`.
  - Added material collection during compile.
  - Primitive leaves emitted material IDs before Material Separation; current primitive output is geometry-only.
  - Current generated GLSL includes `float sceneSDF(vec3 p)` and `SdfMaterialSample sceneMaterial(vec3 p)`.
  - Boolean/domain ops preserve material IDs.
  - Smooth ops now recompute blend weights in `sceneMaterial()` and blend material samples.
  - Implemented `compile(const SdfGraph&)` by building a deterministic temporary tree from graph output links.
  - Graph compile detects cycles and missing node references.
  - Graph compile orders known sockets: `child`, `left`/`base`, `right`/`cutter`, then fallback lexical.

- `assets/shaders/raymarch.frag`
  - Historical note: raymarch now calls `sceneSDF()` in the loop and `sceneMaterial()` after hit.
  - Uses material albedo/emission uniforms with fallback color.
  - Keeps `sceneSDF()` for normal estimation.

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
  - Empty scene fallback now emits `sceneSDF()` plus `sceneMaterial()`.
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

- `src/ui/node_editor/NodeEditor.cpp`
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
  - Added UI split sources: `AddMenu.cpp`, `src/ui/node_editor/*`, `SceneOutliner.cpp`, `PropertiesPanel.cpp`.
  - Added `SdfGraphCompiler.cpp` and `SdfNodeDefinition.cpp`.

## Next Proposed Step

- Deferred Material Eval is implemented:
  - `CompilerSystem` emits per-node `sdf_node_<id>` helpers.
  - `sceneSDF(vec3 p)` is a thin wrapper around the root helper.
  - `sceneMaterial(vec3 p)` evaluates material once after hit confirmation.
  - `raymarch.frag` no longer calls `sceneSDFWithMaterial()`.
- Smooth op material blending is implemented:
  - `sceneMaterial(vec3 p)` returns `SdfMaterialSample`.
  - `raymarch.frag` samples albedo, roughness, metallic, and emission from that struct after hit.
  - SmoothUnion, SmoothIntersect, and SmoothSubtract recompute blend weights inside material evaluation while `sdf_node_<id>` helpers stay `float`.
- Next material candidates:
  - Move from uniform arrays to a material buffer when material count grows beyond 64.
- Other candidates:
  - Split remaining files over hard size gate before unrelated feature work:
    - `src/ui/node_editor/NodeEditorCanvas.cpp` ~330 lines
    - `tests/sdf_node_tests.cpp` ~724 lines
  - Save/load graph schema.
  - Remove migration helper after legacy scene migration period.

## Known Caveat

- Need plan/approval before touching feature code.
- GUI smoke should be rerun after major NodeEditor visual work.
- `SdfNode::material` remains as payload storage for `MaterialOverride` and migration compatibility; do not re-enable primitive material editing/emission.
