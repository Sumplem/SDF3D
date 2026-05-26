# AGENTS MEMORY

## State

- Edit shader `raymarch_edit.frag` active always during editing; `raymarch_scene.frag` for export only; edit overlays force direct preview in path-trace mode
- Canonical branch order: Scale -> Rotate -> Translate; ensure-wrapper reuses existing nodes in chain
- MaterialRegistry owns reusable material assets; each `MaterialDefinition` has a per-material shader graph plus legacy preview `SdfMaterial`
- MaterialOverride has one `sdf` input, applies registry assets by stable `materialId`, and selects registry materials directly from Properties/inline node UI
- MaterialGraph assets compile through `MaterialGraphCompiler` into `SdfMaterialSample` helpers consumed by deferred `sceneMaterial`
- MaterialGraph input defaults live on consuming nodes and are overridden by connected sockets; link compatibility uses socket value type
- MaterialGraph semantic edits mark scene dirty; nodes include core color/float shaping ops, `Checker` color+factor outputs, active `ValueNoise`, and load-only legacy `ValueNoisePattern` migration
- Shared `GraphCanvas`/`GraphEditorCore` own canvas frame behavior and reusable graph drawing/hit primitives
- `GlslEmitter` is 8 concern files; shared math owns domain transforms; primitive emitter supports always-instanced Sphere/Box/Cylinder/Torus/Plane/Capsule/Cone/RoundBox plus legacy SphereInstances
- `SdfNodeDefinition` metadata owns parameter type: Float, Bool, Enum; UI must not infer bool/enum from numeric ranges
- Node groups use App-owned `GraphGroupRegistry`; entered group subgraph is active editor/viewport/compiler/UI target
- Viewport picking uses GPU node-id buffer (`GL_R32I`) and compiler-emitted `scenePickId`; CPU graph raymarch picking is removed
- SDF node auto-layout keeps output-backward columns and assigns rows by DFS subtree ranges; disconnected whole-graph islands stack below the output tree
- SDF graph links allow acyclic DAG fan-out/reuse; `GraphSystem` rejects cycles during link creation and serialized graph load
- Union/SmoothUnion/Intersect/SmoothIntersect use one multi-input `inputs` socket; Viewport Add appends to output-root Union/SmoothUnion when possible
- Runtime GLSL mode reads Float node params from SSBO binding 1; Float edits refresh SSBO only, Bool/Enum/topology recompile
- Active primitives are always instance-capable; eligible translate-only duplicates stay visible in graph and compile into one runtime instance range
- Runtime material/node-param/instance SSBOs bind every frame but re-upload only when renderer data setters mark buffers dirty
- `SdfCompiledNodeParam::data0` stores primitive/transform params; `data1` stores generic instance first/count ranges for runtime helpers
- Primitive UI has no instancing toggle; Properties shows detected instance count read-only
- `SphereInstances` remains load/compile-compatible as legacy data but is hidden from new Add workflows
- Path-trace shader has GI bounces, Russian Roulette, solid environment color, GGX VNDF glossy sampling, NEE/MIS direct light, shader-owned emissive surface sampling, clamped emissive contribution, and accumulation
- Build/tests/smoke: full Debug app build passes; all `build\Debug\*_tests.exe` pass; short hidden app launch stays running

## Active

Always-instanced primitives are ready for review: active primitives implicitly group duplicate translate-only occurrences into runtime instance SSBO data while unsupported branch shapes compile scalar.

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
- 2026-05 - Multi-input boolean sockets keep one visible empty spare slot; drag intent is locked at mouse-down so empty-slot drags cannot detach existing wires
- 2026-05 - Shader highlight is gated by visible GPU pick id plus `sceneNodeContains` so widened scaled-SDF highlight bands do not bleed across multiple objects
- 2026-05 - Inline Translate/Rotate/Scale node-editor edits use node-param SSBO refresh instead of scene shader recompile
- 2026-05 - Viewport Add appends new primitives to an output-root Union or SmoothUnion multi-input instead of creating nested Unions
- 2026-05 - Same-family multi-input boolean nodes can change type in place because Union/SmoothUnion/Intersect/SmoothIntersect share the `inputs` socket contract
- 2026-05 - Procedural noise material nodes use specific algorithm names; first landed node is ValueNoiseMaterial, not generic NoiseMaterial
- 2026-05 - ValueNoiseMaterial reuses material secondary color and patternScale fields so material SSBO layout stays unchanged
- 2026-05 - Runtime param GLSL uses `GlslEmitMode::Runtime` for edit shaders and `GlslEmitMode::Baked` for export shaders so Float edits avoid GPU shader recompile while baked output stays SSBO-free
- 2026-05 - Viewport hover pick readback is cached and throttled because per-frame `glReadPixels` on hover causes GPU/CPU sync stalls; click selection still reads immediately
- 2026-05 - Float parameter UI edits route to param-dirty SSBO refresh; Bool/Enum edits route to scene-dirty because they change GLSL structure
- 2026-05 - Full GI Tier 1 stays renderer/shader-owned: path-trace shader owns Russian Roulette and GGX VNDF sampling, renderer/viewport own solid environment color uniform and accumulation reset
- 2026-05 - GlslEmitter SDF helpers return float distance-only geometry; material IDs live in deferred `sceneMaterial`, not helper return values
- 2026-05 - Boolean material evaluation emits `sceneMaterial` body locals so child distance helpers are evaluated once and reused for material selection/blending
- 2026-05 - Domain transform GLSL expression construction is shared through `GlslEmitterMath` because geometry and deferred material evaluation must stay identical
- 2026-05 - Deferred material emission receives `MaterialSystem` explicitly from compiler orchestration; material registration behavior is not constructed inside `emitMaterialFor` recursion
- 2026-05 - Twist/Bend GLSL default strength and axis values are named constants in `GlslEmitterMath`; avoid raw axis floats in domain emitter logic
- 2026-05 - Legacy material-aware `GlslEmitter::emitNode` path was deleted because compiler output uses float SDF helpers plus deferred `sceneMaterial`
- 2026-05 - Compiled GLSL export writes full injected edit and path-trace fragment sources from App orchestration; renderer remains graph-free
- 2026-05 - Runtime node-param GLSL indexes `uNodeParams[slot].data0` directly; compiler and GraphSystem assign deterministic slots and Baked mode stays SSBO-free
- 2026-05 - `sceneSDFWithId` is the only root-level vec2 distance/id path; per-node `sdf_node_<id>` helpers stay float and `sceneSDF`/`scenePickId` are wrappers
- 2026-05 - Deferred material evaluation threads distance expressions with material samples so boolean material selection does not re-call the same branch helper twice
- 2026-05 - Path-trace direct lighting uses shader-owned next-event estimation with a power-heuristic MIS weight; compiler and MaterialSystem remain uninvolved in GI
- 2026-05 - Path-trace material emission is shader-local and clamped through `emissiveRadiance`; true emissive-object NEE requires explicit design before crossing compiler ownership
- 2026-05 - True emissive-object area-light sampling is deferred until a future LightSystem owns light extraction/query data instead of baking that ownership into GlslEmitter now
- 2026-05 - Material assets own separate per-material shader graphs; scene graph applies them by stable `materialId` through MaterialOverride
- 2026-05 - MaterialGraph compiler emits `SdfMaterialSample` GLSL helpers inside scene GLSL so renderer stays graph-free and material graphs can use hit point `p`
- 2026-05 - Scene Add hides SolidMaterial/CheckerMaterial/ValueNoiseMaterial for new workflows; legacy material source nodes still load and compile for compatibility
- 2026-05 - MaterialGraph input defaults live on the consuming node and compile unless a linked socket overrides that input
- 2026-05 - MaterialGraph canvas follows NodeEditor basics: embedded fields sit below socket labels, right-click adds at cursor, Delete removes selected non-output nodes
- 2026-05 - Shared graph canvas behavior lives in `GraphCanvas`; NodeEditor and MaterialGraphPanel keep graph-specific node/link policy separate
- 2026-05 - Shared graph editor primitives live in `GraphEditorCore`; Scene and Material graph editors reuse drawing/hit primitives but keep graph policy separate
- 2026-05 - MaterialGraph input detaches are preview-only until release because empty release must preserve the existing material graph link
- 2026-05 - MaterialGraph node position edits are layout-only and must not emit material dirty because GPU material refresh depends on SDF tree validity
- 2026-05 - `MultiplyColor` material node emits `mix(a, a * b, factor)` so users can blend between original color and multiplied color
- 2026-05 - MaterialGraph socket names identify endpoints only; link compatibility uses declared socket value types and rejects missing sockets
- 2026-05 - MaterialGraph input drops use row-wide hit testing because embedded input widgets make tiny socket-only drops hard to hit
- 2026-05 - Dragging from an empty MaterialGraph input opens a type-filtered Add popup and auto-links the created node output back to that input
- 2026-05 - MaterialGraph collapsed state is editor data serialized with material graphs and does not affect compiler semantics
- 2026-05 - Graph node hover/selection visual styling is shared in `GraphEditorCore` so scene and material graph canvases stay consistent
- 2026-05 - Graph socket hover radius/color helpers are shared in `GraphEditorCore` so scene and material graph pins stay consistent
- 2026-05 - Link drag drawing and socket hit/drop-feedback primitives are shared in `GraphEditorCore`; graph-specific compatibility and commit policy stay local
- 2026-05 - MaterialOverride direct registry assignment is GraphSystem-owned and clears material input links so UI selection remains authoritative
- 2026-05 - Scene graph no longer has material source nodes; Solid/Checker/ValueNoise live as MaterialRegistry asset presets/MaterialGraph patterns and legacy source nodes migrate on load
- 2026-05 - MaterialGraph `ValueNoise` is a reusable Float factor node; colorizing noise is `MixColor` responsibility, and legacy `ValueNoisePattern` migrates on JSON load
- 2026-05 - MaterialGraph core shaping nodes are graph-owned helpers: `ColorRamp`, `AddColor`, `SubtractColor`, `PowerFloat`, and `ClampFloat`; material asset creation returns to one plain PBR `+ Material`
- 2026-05 - MaterialGraph semantic edits raise scene dirty because graph constants, links, and nodes are compiled into GLSL helpers; names and layout stay non-render semantics
- 2026-05 - MaterialGraph `CheckerPattern` exposes both `color` and `factor` outputs because procedural masks must drive Float inputs directly while still supporting colorized checker output
- 2026-05 - Path-trace emissive surface sampling is shader-owned stochastic next-event estimation over visible emissive SDF hits; no compiler, MaterialSystem, or LightSystem ownership is added in this slice
- 2026-05 - MaterialGraphPanel keeps material assets in a right-side list while the left side behaves like the scene node graph canvas; node creation stays in the graph popup instead of a top button grid
- 2026-05 - SDF node auto-layout rows are DFS-assigned from Output backward; each child subtree owns its row range, and whole-graph disconnected islands stack below the output-connected tree
- 2026-05 - SDF graphs support acyclic DAG fan-out so one source can feed direct and transformed branches; `GraphSystem` rejects cycles on link creation and graph data replacement
- 2026-05 - Capsule, Cone, and RoundBox are active primitive SDF nodes with metadata-driven UI and baked/runtime GLSL emission
- 2026-05 - `SphereInstances` uses one node-param slot for radius plus instance-buffer first/count so adding positions refreshes SSBOs without shader recompile
- 2026-05 - Runtime instance positions use renderer-owned SSBO binding 2; baked GLSL emits literal positions and stays SSBO-free
- 2026-05 - Runtime SSBO uploads are dirty-driven: renderer data setters mark buffers dirty, render frames bind buffers but skip `glBufferData` when data is unchanged
- 2026-05 - SphereInstances batch additions are Properties-panel data edits only; they append positions and refresh runtime buffers without changing graph topology
- 2026-05 - SphereInstances position rows are virtualized in Properties because drawing one ImGui row per instance makes add/edit latency scale with total instance count
- 2026-05 - Viewport Add SphereInstances reuses the selected SphereInstances node as an append target because adding one position is instance data, not graph topology
- 2026-05 - Generic primitive instancing is compile-time optimization: marked duplicate graph branches remain editable while eligible translate-only occurrences collapse into one runtime instance range
- 2026-05 - Instanced primitive runtime params use `SdfCompiledNodeParam::data1` for first/count so `data0` can keep full primitive parameter payloads across all primitive shapes
- 2026-05 - `SphereInstances` stays legacy-load compatible but is hidden from new Add workflows because generic primitive instancing replaces the user-facing node
- 2026-05 - Active primitives are always instance-capable by definition; `instancePrototypeId == 0` means self prototype and duplicates inherit source prototype identity
- 2026-05 - Primitive instancing UI is read-only because instancing is no longer an opt-in graph state
- 2026-05 - Normal primitive serialization omits instancing metadata; legacy primitive instancing metadata is tolerated and ignored on load

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
- Do not remap Subtract/SmoothSubtract in boolean type-change UI without explicit socket conversion rules because their base/cutter sockets differ from multi-input `inputs`
- Do not tint highlights by distance alone because multi-object scenes can bleed highlight onto nearby unselected objects
- Do not add a generic NoiseMaterial node because multiple noise algorithms will coexist and names must stay specific
- Do not route material graph ownership through renderer because material graph compilation belongs to compiler/UI layers
- Do not reintroduce scene graph material source nodes because material assets belong in MaterialRegistry/MaterialGraph and MaterialOverride references them by `materialId`
- Never allow cyclic SDF graph links because compiler/layout/validity traversals assume directed acyclic graph flow
- Keep instancing position data compiler/scene-owned and upload only compiled buffers to renderer because renderer must stay graph-free

## Environment

- Build: `cmake --build build --config Debug`
- Test executables: `build\Debug\<target>.exe` (Windows); `build/<target>` (Linux/Mac)
- CMake 4.3+: must set `JSON_BuildTests OFF` and `JSON_Install OFF` before `FetchContent_MakeAvailable`
- GLAD regen requires real Python interpreter; needs `jinja2` and `MarkupSafe` installed
- Python path is machine-specific; pass via `-DPython_EXECUTABLE=<path>` at configure time; never hardcode
- Always use repo root as working directory; never assume absolute paths
- `LNK1168` = sdf3d.exe still running; stop process before rebuild

## Next

Review always-instanced primitive duplicate behavior in-app; next focus is interaction polish if duplicate/selection UX needs tightening.
