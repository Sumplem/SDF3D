# AGENTS MEMORY

## State

- Edit shader `raymarch_edit.frag` active always during editing; `raymarch_scene.frag` for export only
- Translate/Rotate/Scale gizmos live; edit gizmos/highlight force direct preview even when path-trace mode is selected
- Viewport picking uses `GraphSystem::pickNodeByRay()` and chain-aware final visible transform selection
- Selection highlight uses `uHighlightNodeId` + `sceneNodeSDF(int nodeId, vec3 p)` with smooth distance band
- Canonical branch order: Scale -> Rotate -> Translate; ensure-wrapper reuses existing nodes in chain
- `SdfNodeTraits.h` centralizes node taxonomy; `GraphSystemTransforms.cpp` owns transform wrapper logic
- `GlslEmitter` consolidated to 8 files by engineering concern: dispatch, primitives, booleans, domain, materials, scene assembly, math, formatting
- Plane primitive normal components stay user-editable; emitter normalizes with `normalize(vec3(...))` in GLSL
- `SdfNodeDefinition` metadata owns parameter type: Float, Bool, Enum; UI must not infer bool/enum from numeric ranges
- Repeat/Mirror toggles use Bool metadata; Twist/Bend axis uses Enum metadata with X=0, Y=1, Z=2
- MaterialRegistry owns reusable graph materials; SolidMaterial/CheckerMaterial nodes hold stable `materialId`
- MaterialOverride consumes `sdf` + `material`; inline material fallback remains legacy-only
- Material SSBO binding=0; node param SSBO binding=1; no material cap
- M6 path-tracing infrastructure exists: `RenderMode`, lazy `raymarch_pathtrace.frag`, HDR `PathTraceAccumulation`, sample reset keys
- Path-trace shader has stochastic GI bounces, cosine hemisphere sampling, direct light shadow checks, emissive contribution, and progressive accumulation
- Path-trace accumulation resets on camera, viewport, scene, material, node-param, quality, and mode changes
- Save/load JSON uses `GraphSerializer` interface and `JsonGraphSerializer`; nlohmann/json pinned
- GraphSerializer tests verify MaterialRegistry round-trip reaches compiled material output after load
- Shared graph validity lives in `GraphSystemValidity.cpp`; compiler and UI consume GraphSystem queries
- Incomplete-node bypass visuals use `GraphSystem::effectiveBypassSourceLink()` instead of UI-local rules
- Properties panel has Material Palette with swatch, rename, and safe delete for orphan registry materials
- GraphSystem owns MaterialRegistry rename/delete safety; referenced materials cannot be deleted
- JSON graph serialization is clean: stableId assigned/restored, materialId preserved, sockets/material blobs/selection omitted
- JSON load migrates old material blobs and stale socket/selection fields silently
- Root `sdf3d_graph.json` sample uses clean scene schema with no selection, socket arrays, node material blobs, or zero stable IDs
- Deleting unreferenced material source nodes removes their registry entries; MaterialOverride references keep registry entries alive
- Build/tests: full Debug build pass; all test executables pass; GUI smoke pass

## Active

JSON serialization correctness batch complete: six fixes landed, clean JSON verified by serializer tests, and review gate open.

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
- 2026-05 - SdfNodeDefinition parameter metadata is typed; Bool and Enum drive UI widgets instead of numeric-range or node-type heuristics
- 2026-05 - Path trace quality is renderer-owned; viewport Quality maps to max bounces while shader owns stochastic sampling and accumulation
- 2026-05 - Path-trace denoise starts lightweight in shader: jitter samples for anti-aliasing and clamp high radiance before progressive accumulation
- 2026-05 - Path-trace sample count is renderer-owned state exposed read-only to viewport UI
- 2026-05 - Path-trace glossy rays use shader-owned roughness-aware importance sampling; renderer/compiler contracts unchanged
- 2026-05 - Path-trace temporal denoise remains shader-only for now; no extra accumulation textures or compiler/material ownership
- 2026-05 - Renderer exposes last-frame path-trace activity read-only so UI can report accumulation vs edit preview without duplicating renderer rules
- 2026-05 - Spatial denoise stays shader-local and uses only the existing accumulation texture; no extra GL targets yet
- 2026-05 - MaterialRegistry save/load verification must prove loaded registry data reaches compiler output, not only JSON fields
- 2026-05 - Graph validity and incomplete-node bypass source selection are GraphSystem-owned; compiler and UI consume shared queries
- 2026-05 - MaterialRegistry mutation safety is GraphSystem-owned; UI cannot delete materials still referenced by graph nodes
- 2026-05 - Scene JSON stores graph state only; selection is ephemeral UI state and sockets are reconstructed from SdfNodeDefinition
- 2026-05 - Node material payload is legacy migration data only; saved JSON stores registry material data plus node materialId references

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

Review JSON serialization correctness batch, then next backlog item is viewport picking improvement.
