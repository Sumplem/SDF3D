#pragma once

#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/scene/SdfCompiler.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sdf3d {

class EventBus;
class GraphGroupRegistry;

/// Runtime buffers refreshed without shader recompilation.
struct SdfRuntimeBufferData {
    std::vector<SdfCompiledNodeParam> nodeParams;
    SdfCompiledInstanceData instances;
};

/// Owns SDF graph mutation and validation rules.
class GraphSystem {
public:
    /// Creates the permanent graph output node.
    static void initialize(SdfGraph& graph);

    /// Creates a graph node and returns its stable ID.
    static SdfGraphNodeId createNode(SdfGraph& graph, SdfNodeType type, std::string name = {});

    /// Duplicates one graph node payload and editor state without copying links.
    static SdfGraphNodeId duplicateNode(SdfGraph& graph, SdfGraphNodeId id);

    /// Duplicates selected nodes and preserves links wholly inside the selection.
    static std::vector<SdfGraphNodeId> duplicateSelection(SdfGraph& graph, const std::vector<SdfGraphNodeId>& ids, EventBus& eventBus);

    /// Renames one graph node and syncs linked registry-owned labels.
    static bool renameNode(SdfGraph& graph, GraphGroupRegistry& groups, SdfGraphNodeId id, std::string name);

    /// Changes a compatible graph node type while preserving id, links, and editor state.
    static bool changeNodeType(SdfGraph& graph, SdfGraphNodeId id, SdfNodeType type);

    /// Returns node display name, resolving registry-backed labels.
    static std::string displayNameForNode(const SdfGraphNode& node, const GraphGroupRegistry& groups);

    /// Creates a group definition from selected self-contained nodes and replaces them with one Group instance.
    static SdfGraphNodeId groupSelection(
        SdfGraph& graph,
        GraphGroupRegistry& groups,
        const std::vector<SdfGraphNodeId>& ids,
        SdfGraphNodeId primary,
        std::string name);

    /// Replaces graph internals after validating loaded serialized data.
    static bool replaceGraphData(
        SdfGraph& graph,
        SdfGraphNodeId nextId,
        SdfGraphNodeId outputNode,
        SdfGraphNodeId selectedNode,
        std::vector<SdfGraphNodeId> selectedNodes,
        std::unordered_map<SdfGraphNodeId, SdfGraphNode> nodes,
        std::vector<SdfGraphLink> links);

    /// Deletes a node and all links connected to it.
    static bool deleteNode(SdfGraph& graph, SdfGraphNodeId id);

    /// Renames one registry material by stable ID.
    static bool renameMaterial(SdfGraph& graph, MaterialId id, std::string name);

    /// Returns true when no graph node references the registry material.
    static bool canDeleteMaterial(const SdfGraph& graph, MaterialId id);

    /// Deletes one unreferenced registry material by stable ID.
    static bool deleteMaterial(SdfGraph& graph, MaterialId id);

    /// Assigns a registry material to a MaterialOverride node.
    static bool assignMaterialToNode(SdfGraph& graph, SdfGraphNodeId nodeId, MaterialId materialId);

    /// Returns detected graph occurrences for a primitive instance prototype.
    static std::size_t primitiveInstanceCount(const SdfGraph& graph, SdfGraphNodeId nodeId);

    /// Appends one world-space instance position to a SphereInstances node.
    static bool appendInstancePosition(SdfGraph& graph, SdfGraphNodeId nodeId, glm::vec3 position);

    /// Connects one node output to a named input socket on another node.
    static bool link(SdfGraph& graph, SdfGraphNodeId fromNode, SdfGraphNodeId toNode, std::string toSocket);

    /// Connects one named output socket to one named input socket.
    static bool link(SdfGraph& graph, SdfGraphNodeId fromNode, std::string fromSocket, SdfGraphNodeId toNode, std::string toSocket);

    /// Removes all links targeting a node socket.
    static bool unlinkInput(SdfGraph& graph, SdfGraphNodeId toNode, const std::string& toSocket);

    /// Removes one exact link between two sockets.
    static bool unlink(SdfGraph& graph, SdfGraphNodeId fromNode, const std::string& fromSocket, SdfGraphNodeId toNode, const std::string& toSocket);

    /// Sets the graph node used as the render output.
    static bool setOutputNode(SdfGraph& graph, SdfGraphNodeId id);

    /// Returns true when the node is the permanent graph output marker.
    static bool isOutputNode(const SdfGraph& graph, SdfGraphNodeId id);

    /// Returns true when any link starts from or ends at the node.
    static bool hasLinks(const SdfGraph& graph, SdfGraphNodeId id);

    /// Returns true when the graph node can emit an effective SDF output.
    static bool producesValidSdf(const SdfGraph& graph, SdfGraphNodeId id);

    /// Returns an input link only if its upstream node emits an effective SDF.
    static std::optional<SdfGraphLink> effectiveLinkToInput(const SdfGraph& graph, SdfGraphNodeId id, const std::string& socket);

    /// Returns input links only if their upstream nodes emit effective SDF.
    static std::vector<SdfGraphLink> effectiveLinksToInput(const SdfGraph& graph, SdfGraphNodeId id, const std::string& socket);

    /// Returns true when a node is missing an effective required SDF input.
    static bool nodeHasMissingRequiredInput(const SdfGraph& graph, const SdfGraphNode& node);

    /// Returns the effective source link used when an incomplete node visually bypasses itself.
    static std::optional<SdfGraphLink> effectiveBypassSourceLink(const SdfGraph& graph, const SdfGraphNode& node);

    /// Returns true when lowered children satisfy graph validity rules.
    static bool loweredNodeHasRequiredInputs(SdfNodeType type, const std::vector<std::string>& validSockets, std::size_t childCount);

    /// Packs graph node parameters for renderer-side fast param updates.
    static std::vector<SdfCompiledNodeParam> collectNodeParams(const SdfGraph& graph);

    /// Packs graph node parameters from the graph plus reachable group definitions.
    static std::vector<SdfCompiledNodeParam> collectNodeParams(const SdfGraph& graph, const GraphGroupRegistry& groups);

    /// Packs instanced primitive positions for renderer-side fast instance updates.
    static SdfCompiledInstanceData collectInstanceData(const SdfGraph& graph);

    /// Packs instanced primitive positions from the graph plus reachable group definitions.
    static SdfCompiledInstanceData collectInstanceData(const SdfGraph& graph, const GraphGroupRegistry& groups);

    /// Packs node parameters and instance ranges in one graph traversal pass.
    static SdfRuntimeBufferData collectRuntimeBufferData(const SdfGraph& graph);

    /// Packs node parameters and instance ranges from graph plus reachable group definitions in one pass.
    static SdfRuntimeBufferData collectRuntimeBufferData(const SdfGraph& graph, const GraphGroupRegistry& groups);

    /// Returns the SDF helper node ID that should be highlighted for current selection.
    static SdfGraphNodeId highlightNodeForSelection(const SdfGraph& graph);

    /// Returns the SDF helper node ID that should be highlighted for a picked node.
    static SdfGraphNodeId highlightNodeForNode(const SdfGraph& graph, SdfGraphNodeId id);

    /// Returns direct Translate parent connected to node's `sdf` output, or 0.
    static SdfGraphNodeId findDirectTranslateParent(const SdfGraph& graph, SdfGraphNodeId id);

    /// Returns direct Rotate parent connected to node's `sdf` output, or 0.
    static SdfGraphNodeId findDirectRotateParent(const SdfGraph& graph, SdfGraphNodeId id);

    /// Returns direct Scale parent connected to node's `sdf` output, or 0.
    static SdfGraphNodeId findDirectScaleParent(const SdfGraph& graph, SdfGraphNodeId id);

    /// Reuses or creates a Translate wrapper for a primitive node and selects it.
    static SdfGraphNodeId ensureTranslateWrapperForNode(SdfGraph& graph, SdfGraphNodeId id);

    /// Reuses or creates a Rotate wrapper for a primitive node and selects it.
    static SdfGraphNodeId ensureRotateWrapperForNode(SdfGraph& graph, SdfGraphNodeId id);

    /// Reuses or creates a Scale wrapper for a primitive node and selects it.
    static SdfGraphNodeId ensureScaleWrapperForNode(SdfGraph& graph, SdfGraphNodeId id);

    /// Computes effective Translate offset upstream of a Translate node through unary SDF pass-through nodes.
    static glm::vec3 accumulatedTranslatePosition(const SdfGraph& graph, SdfGraphNodeId translateId);

    /// Wraps primitive, places it, and unions it with current Output surface if needed.
    static SdfGraphNodeId placePrimitiveAtWorldPosition(SdfGraph& graph, SdfGraphNodeId primitiveNode, glm::vec3 worldPosition);
};

} // namespace sdf3d
