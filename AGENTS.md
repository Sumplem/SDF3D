# AGENTS MEMORY

## State

- Edit shader `raymarch_edit.frag` active always during editing; `raymarch_scene.frag` for export only
- Viewport picking: `GraphSystem::pickNodeByRay()` per boolean branch; returns nearest branch transform
- Selection highlight: `uHighlightNodeId` + `sceneNodeSDF(int nodeId, vec3 p)`; smooth distance band
- Translate/Rotate/Scale gizmos live; Rotate has `GizmoRotateStyle` with AxisArcs selectable stub falling back to rings
- Scale gizmo uses edit-shader box SDF, CPU SDF hit-test, canonical Scale wrapper insertion, and runtime SSBO scale params
- Translate gizmo uses edit-shader SDF capsules + CPU SDF raymarch hit-test; no GL lines/draw calls
- Rotate: hidden `qx/qy/qz/qw` quaternion; Euler degrees = UI adapter only
- Quaternion rotate helper assumes normalized CPU input; shader has no `normalize(q)`; products precomputed in GLSL
- Canonical branch order: Scale -> Rotate -> Translate; ensure-wrapper reuses existing nodes in chain
- `SdfNodeTraits.h` centralizes node taxonomy; replaces all duplicated predicates
- `GraphSystemTransforms.cpp` owns transform wrapper logic; `GraphSystem.cpp` owns CRUD + param collection
- `GlslEmitter` consolidated to 8 files by engineering concern: dispatch, primitives, booleans, domain, materials, scene assembly, math, formatting
- MaterialRegistry owns reusable graph materials; SolidMaterial/CheckerMaterial nodes hold stable `materialId`
- MaterialOverride consumes `sdf` + `material`; it applies material nodes to geometry and keeps inline fallback only for legacy files
- Procedural materials support SolidMaterial and CheckerMaterial; material SSBO packs type, secondary color, and pattern scale
- JSON load repairs legacy or partial source material nodes by creating missing MaterialRegistry entries
- Material SSBO binding=0; node param SSBO binding=1; no material cap
- M6 path-tracing infrastructure exists: `RenderMode`, lazy `raymarch_pathtrace.frag`, HDR `PathTraceAccumulation`, sample reset keys
- Path trace mode is an explicit viewport toggle; edit gizmo/highlight forces direct preview so editing handles stay visible
- Current path-trace shader is direct-light accumulation stub only; real stochastic bounces/BRDF sampling not landed yet
- Cook-Torrance GGX; soft shadows; AO; `NORMAL_EPSILON = 0.00035`
- Save/load JSON; `GraphSerializer` interface; `JsonGraphSerializer`; nlohmann/json pinned
- Phase 2 ops complete: Repeat axis toggles, Twist/Bend axis combo, warp correction, material-space transform mirror
- Node inline property widgets scale font/style with canvas zoom
- Parameter visibility lives in `SdfNodeDefinition` metadata; UI must not hardcode hidden rotate params
- Build/tests: full Debug build pass; all test executables pass; GUI smoke pass

## Active

Material split complete: SolidMaterial and CheckerMaterial are separate material source nodes; MaterialOverride applies a material input to SDF input. Review gate open.

## Decisions

- 2026 - ECS architecture: components=data; systems=logic; renderer=GL; app=wiring
- 2026 - SDF node tree is AST, compiled to GLSL per graph change
- 2026 - Material is explicit MaterialOverride node, not embedded in primitive
- 2026 - Deferred material eval: sceneSDF for march loop; sceneMaterial once at hit
- 2026 - Smooth op material blending: recompute smin weight in sceneMaterial; sdf_node helpers stay float
- 2026 - Runtime node params via SSBO binding=1; gizmo drags write params without shader recompile
- 2026 - Rotate stores quaternion internally; Euler degrees are UI-only adapter
- 2026 - Gizmo hit-test: CPU-side SDF raymarch; same formula as shader; nearest hit wins
- 2026 - Two shaders: raymarch_edit.frag (always during edit); raymarch_scene.frag (export only)
- 2026 - GlslEmitter split rule: one file per engineering concern (reason to change) not per code path
- 2026 - Agent memory: 6 sections only; no Recent Approved Edits; no absolute paths
- 2026-05 - Rotate gizmo style is renderer state via `GizmoRotateStyle`; AxisArcs starts as explicit stub falling back to rings
- 2026-05 - Scale gizmo uses box SDF render/pick and packs x/y/z/min-axis into runtime node params
- 2026-05 - Phase 2 ops use metadata bool/axis UI; Repeat disables axes by skipping `mod`, Twist/Bend choose axis and apply warp correction in geometry
- 2026-05 - MaterialRegistry is graph-owned; MaterialOverride stores stable `materialId`; renderer still receives packed compile-time material slots
- 2026-05 - Procedural material data stays in `SdfMaterial`; shader samples checker from world-space `p` through `sampleMaterial(materialId, p)`
- 2026-05 - Material source nodes are separate from MaterialOverride; SolidMaterial/CheckerMaterial output `material`, MaterialOverride consumes `sdf` + `material`
- 2026-05 - Progressive path tracing starts as renderer-only infrastructure: no compiler/material-system ownership, reset accumulation on camera/scene/material/node-param/quality/mode changes

## Constraints

- Never disable single-valid-input boolean bypasses because intentional compiler + UI behavior
- Never re-enable primitive material editing/emission because `SdfNode::material` is MaterialOverride fallback cache only
- Renderer never includes SdfGraph.h or any scene/component header because ECS Law 3
- Never swap to raymarch_scene.frag on deselect because swap only for explicit render/export
- Always preserve canonical branch order Scale -> Rotate -> Translate in ensure-wrapper logic
- Never normalize quaternion in shader because CPU normalizes before SSBO upload
- Keep path-tracing work renderer/shader-owned; do not add GI behavior to compiler or MaterialSystem
- Do not hide edit gizmos/highlight for path tracing; direct preview wins while editing overlays are active
- Never split files unilaterally because user approval is required
- Never add new GlslEmitter features until consolidation to 8-file structure is complete
- Do not split tests/test_sdf_graph.cpp because user declined

## Environment

- Build: `cmake --build build --config Debug`
- Test executables: `build\Debug\<target>.exe` (Windows); `build/<target>` (Linux/Mac)
- CMake 4.3+: must set `JSON_BuildTests OFF` and `JSON_Install OFF` before `FetchContent_MakeAvailable`
- GLAD regen requires real Python interpreter; needs `jinja2` and `MarkupSafe` installed
- Python path is machine-specific; pass via `-DPython_EXECUTABLE=<path>` at configure time; never hardcode
- Always use repo root as working directory; never assume absolute paths
- `LNK1168` = sdf3d.exe still running; stop process before rebuild

## Next

Review material split, then next backlog item is full GI/path tracing design before code.
