# AGENTS MEMORY

## State

- Edit shader `raymarch_edit.frag` active always during editing; `raymarch_scene.frag` for export only
- Translate/Rotate/Scale gizmos live; edit gizmos/selection highlight/GPU hover highlight force direct preview even when path-trace mode is selected
- Canonical branch order: Scale -> Rotate -> Translate; ensure-wrapper reuses existing nodes in chain
- `SdfNodeTraits.h` centralizes node taxonomy; `GraphSystemTransforms.cpp` owns transform wrapper logic
- `GlslEmitter` consolidated to 8 files by engineering concern: dispatch, primitives, booleans, domain, materials, scene assembly, math, formatting
- `SdfNodeDefinition` metadata owns parameter type: Float, Bool, Enum; UI must not infer bool/enum from numeric ranges
- MaterialRegistry owns reusable graph materials; SolidMaterial/CheckerMaterial nodes hold stable `materialId`
- MaterialOverride consumes `sdf` + `material`; inline material fallback remains legacy-only
- Node groups exist: App-owned `GraphGroupRegistry`, `SdfNodeType::Group`, root JSON `definitions`, Ctrl+G grouping shortcut
- Node editor supports group navigation: Tab or double-click enters selected/group node, Shift+Tab exits, breadcrumb switches active graph, clears transient editor state, and marks scene dirty
- Entered group subgraph is active editor/viewport/compiler/UI Add/SceneOutliner/Properties target; viewport picking/gizmos/Add use active graph
- Group node rename syncs its registry definition name; Group display names resolve through the definition for titles, outliner, properties, breadcrumbs, and scope labels
- Group compile resolves definitions through `CompilerSystem`/`MaterialSystem`; reachable group transform params are uploaded for runtime edits
- Group-internal lowered nodes use scoped runtime IDs so helper names and `sceneNodeSDF` switch labels cannot collide with root graph IDs
- Viewport picking uses GPU node-id buffer (`GL_R32I`) and single-pixel reads; CPU graph raymarch picking is removed
- `scenePickId(vec3 p)` is compiler-emitted with scene GLSL; groups pick as root Group instance IDs
- Viewport hover highlight reads the GPU node-id buffer on mouse move, then maps picked node through the same highlight target logic as selection
- Group definitions have one sdf output only; exposed params, library sharing, and Make Unique remain deferred
- Save JSON writes only group definitions reachable from root group instances; nested reachable definitions are regression-covered
- Load Graph commits root graph and group registry atomically, then resets NodeEditor active graph navigation to root before recompiling loaded scene
- Union/SmoothUnion/Intersect/SmoothIntersect use one vertical pill-shaped `inputs` multi-input SDF socket; pill grows with incoming wire count, each wire gets a separate anchor, and dragging from pill detaches nearest wire
- Path-trace shader has stochastic GI bounces, cosine hemisphere sampling, direct light shadow checks, emissive contribution, and progressive accumulation
- Shared graph validity lives in `GraphSystemValidity.cpp`; compiler and UI consume GraphSystem queries; no remaining local UI bypass rules found
- README reflects current editor/compiler/renderer/test state
- Viewport camera supports Shift+right-click drag pan; right-click drag orbit and right-click release Add popup remain intact
- Build/tests: full Debug build pass; all test executables pass; GUI smoke not rerun for multi-input slice

## Active

Multi-input socket complete: boolean merge nodes now use one vertical pill-shaped `inputs` multi-input socket, UI can add/unplug multiple wires without replacing existing ones, the pill grows per wire with separate anchors, and old `left`/`right` JSON links migrate on load. Review gate open.

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
- 2026-05 - GraphGroupRegistry is App-owned and serialized at scene root; Group instance nodes store only definitionId
- 2026-05 - Group compilation resolves definitions through compiler/material-system overloads, keeping renderer unaware of graph/group data
- 2026-05 - NodeEditor active graph can be root or group subgraph; Add menu targets the active graph, while viewport Add remains root graph
- 2026-05 - When inside a group, renderer compile/material collection and viewport picking use the active group subgraph instead of root graph
- 2026-05 - Main menu Add, SceneOutliner, Properties panel, viewport picking, viewport Add, duplicate, compile, and material refresh all target the NodeEditor active graph
- 2026-05 - Group instance rename updates its registry definition name because breadcrumbs and active scope labels read definition names
- 2026-05 - Group node display labels resolve through `GraphGroupDefinition::name`; local node payload name is not authoritative for visible Group names
- 2026-05 - Double-clicking a Group node title is an editor navigation shortcut equivalent to selecting it and pressing Tab
- 2026-05 - Group definitions are serialized only when reachable from the saved root graph; orphan definitions stay runtime-only until referenced again
- 2026-05 - Loading a scene resets NodeEditor active graph navigation to root because group definition IDs are scene-local
- 2026-05 - Breadcrumb graph navigation clears transient NodeEditor state because node and socket IDs are graph-local
- 2026-05 - Grouped scene load commits root graph and group registry atomically because definitions and graph instances are one serialized unit
- 2026-05 - Nested group definitions are part of the reachable scene closure and must round-trip with the root graph
- 2026-05 - Group selection rejects multiple external outputs because one Group node exposes one sdf output only
- 2026-05 - Viewport picking resolves group definitions but returns the root Group instance so root graph selection stays editable
- 2026-05 - Group graph compile collects reachable definition transform params because runtime node params must match emitted GLSL helper IDs
- 2026-05 - Group-internal lowered node IDs are scoped by definition ID because subgraph node IDs can collide with root graph node IDs
- 2026-05 - Viewport hover highlight is renderer uniform state separate from graph selection because hover preview must not mutate selected nodes
- 2026-05 - Viewport boolean-branch picking evaluates every branch and selects smallest ray distance because viewport selection must be depth-ordered
- 2026-05 - Hover highlight is click-scoped viewport state because mouse-move raymarching is too expensive for editor interaction
- 2026-05 - GPU node-id picking replaces CPU graph raymarch picking; `scenePickId` owns branch winner ids and the edit FBO owns `GL_R32I` node-id readback
- 2026-05 - Hover node-id readback maps through `GraphSystem::highlightNodeForNode` because raw pick ids may be leaf nodes behind visible transform wrappers
- 2026-05 - Viewport pan is UI-owned camera behavior: Shift+right-click drag moves the orbit target in camera plane without changing orbit angles or distance
- 2026-05 - Boolean merge nodes use one `inputs` multi-input SDF socket because graph cardinality belongs to socket metadata, not duplicated `left`/`right` UI sockets

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
- Do not add group exposed params, Make Unique, or library sharing until explicitly requested because node group M6+ deferred scope

## Environment

- Build: `cmake --build build --config Debug`
- Test executables: `build\Debug\<target>.exe` (Windows); `build/<target>` (Linux/Mac)
- CMake 4.3+: must set `JSON_BuildTests OFF` and `JSON_Install OFF` before `FetchContent_MakeAvailable`
- GLAD regen requires real Python interpreter; needs `jinja2` and `MarkupSafe` installed
- Python path is machine-specific; pass via `-DPython_EXECUTABLE=<path>` at configure time; never hardcode
- Always use repo root as working directory; never assume absolute paths
- `LNK1168` = sdf3d.exe still running; stop process before rebuild

## Next

After multi-input socket review, choose next backlog/design item.
