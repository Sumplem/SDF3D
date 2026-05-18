#pragma once

#include "sdf3d/scene/SdfGraph.h"

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace sdf3d {

class EventBus;

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

    /// Returns direct Translate parent connected to node's `sdf` output, or 0.
    static SdfGraphNodeId findDirectTranslateParent(const SdfGraph& graph, SdfGraphNodeId id);

    /// Reuses or creates a Translate wrapper for a primitive node and selects it.
    static SdfGraphNodeId ensureTranslateWrapperForNode(SdfGraph& graph, SdfGraphNodeId id);

    /// Computes effective Translate offset upstream of a Translate node through unary SDF pass-through nodes.
    static glm::vec3 accumulatedTranslatePosition(const SdfGraph& graph, SdfGraphNodeId translateId);

    /// Wraps primitive, places it, and unions it with current Output surface if needed.
    static SdfGraphNodeId placePrimitiveAtWorldPosition(SdfGraph& graph, SdfGraphNodeId primitiveNode, glm::vec3 worldPosition);
};

} // namespace sdf3d
