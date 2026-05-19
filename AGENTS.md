# AGENTS MEMORY

## State

- Edit shader `raymarch_edit.frag` active always during editing · `raymarch_scene.frag` for export only
- Viewport picking: `GraphSystem::pickNodeByRay()` per boolean branch · returns nearest branch transform
- Selection highlight: `uHighlightNodeId` + `sceneNodeSDF(int nodeId, vec3 p)` · smooth distance band
- Translate/Rotate/Scale gizmos live · oriented by branch Rotate · runtime param SSBO binding=1 · drag = no recompile
- Rotate: hidden `qx/qy/qz/qw` quaternion · Euler degrees = UI adapter only
- Canonical branch order: Scale → Rotate → Translate · ensure-wrapper reuses existing nodes in chain
- `SdfNodeTraits.h` centralizes node taxonomy · replaces all duplicated predicates
- `GraphSystemTransforms.cpp` owns transform wrapper logic · `GraphSystem.cpp` owns CRUD + param collection
- `GlslEmitter` currently 9 files (over-split by code path) · target 8 files by engineering concern · consolidation approved not started
- Material SSBO binding=0 · node param SSBO binding=1 · no material cap
- Cook-Torrance GGX · soft shadows · AO · `NORMAL_EPSILON = 0.00035`
- Save/load JSON · `GraphSerializer` interface · `JsonGraphSerializer` · nlohmann/json pinned
- Phase 2 ops: Repeat · Mirror · Twist · Bend (artifacts + axis config pending)
- Auto layout: graph-traversal · parent-row-ordered · centered columns · empty slot preservation
- Build: `cmake --build build --config Debug` ✅
- Tests: all focused test executables pass ✅ · GUI smoke ✅

---

## Active

GlslEmitter consolidation — priority 1. Reorganizing 9 current files into 8 files each with one engineering reason to change. Not started. Must complete before any new emitter features.

---

## Decisions

- 2026 — ECS architecture: components=data · systems=logic · renderer=GL · app=wiring
- 2026 — SDF node tree is AST, compiled to GLSL per graph change
- 2026 — Material is explicit MaterialOverride node, not embedded in primitive
- 2026 — Deferred material eval: sceneSDF for march loop · sceneMaterial once at hit
- 2026 — Smooth op material blending: recompute smin weight in sceneMaterial · sdf_node helpers stay float
- 2026 — Runtime node params via SSBO binding=1 · gizmo drags write params without shader recompile
- 2026 — Rotate stores quaternion internally · Euler degrees are UI-only adapter
- 2026 — Gizmo hit-test: CPU-side SDF raymarch · same formula as shader · nearest hit wins
- 2026 — Two shaders: raymarch_edit.frag (always during edit) · raymarch_scene.frag (export only)
- 2026 — GlslEmitter split rule: one file per engineering concern (reason to change) not per code path
- 2026 — Agent memory: 6 sections only · no Recent Approved Edits · no absolute paths

---

## Constraints

- Never disable single-valid-input boolean bypasses — intentional compiler + UI behavior
- Never re-enable primitive material editing/emission — `SdfNode::material` is MaterialOverride payload only
- Renderer never includes SdfGraph.h or any scene/component header — ECS Law 3
- Never swap to raymarch_scene.frag on deselect — only swap for explicit render/export
- Always preserve canonical branch order Scale → Rotate → Translate in ensure-wrapper logic
- Never normalize quaternion in shader — normalize on CPU before SSBO upload
- Never split files unilaterally — propose + state two concerns + wait for approval
- Never add new GlslEmitter features until consolidation to 8-file structure is complete
- Do not split tests/test_sdf_graph.cpp — user declined, do not reopen

---

## Environment

- Build: `cmake --build build --config Debug`
- Test executables: `build\Debug\<target>.exe` (Windows) · `build/<target>` (Linux/Mac)
- CMake 4.3+: must set `JSON_BuildTests OFF` and `JSON_Install OFF` before `FetchContent_MakeAvailable`
- GLAD regen requires real Python interpreter · needs `jinja2` and `MarkupSafe` installed
- Python path is machine-specific — pass via `-DPython_EXECUTABLE=<path>` at configure time · never hardcode
- Always use repo root as working directory · never assume absolute paths
- `LNK1168` = sdf3d.exe still running · stop process before rebuild

---

## Next

Begin GlslEmitter consolidation: read all 9 current glsl_emitter files, map every function to its target file in the 8-file structure, show full mapping table, wait for approval before touching any file.