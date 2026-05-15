#pragma once

#include "sdf3d/scene/SdfNode.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sdf3d {

/// Stable identifier for a node inside an SDF graph.
using SdfGraphNodeId = uint64_t;

/// Value category accepted by a graph socket.
enum class SdfSocketType {
    Sdf,
    Float,
    Vector3,
    Material,
};

/// Direction of data flow for a graph socket.
enum class SdfSocketDirection {
    Input,
    Output,
};

/// Declares a typed input or output socket on a graph node.
struct SdfGraphSocket {
    std::string name;
    SdfSocketType type = SdfSocketType::Sdf;
    SdfSocketDirection direction = SdfSocketDirection::Input;
    bool multiInput = false;
};

/// Directed connection from one node output into one node input socket.
struct SdfGraphLink {
    SdfGraphLink() = default;

    SdfGraphLink(SdfGraphNodeId from, SdfGraphNodeId to, std::string inputSocket)
        : fromNode(from)
        , toNode(to)
        , toSocket(std::move(inputSocket))
    {
    }

    SdfGraphLink(SdfGraphNodeId from, std::string outputSocket, SdfGraphNodeId to, std::string inputSocket)
        : fromNode(from)
        , fromSocket(std::move(outputSocket))
        , toNode(to)
        , toSocket(std::move(inputSocket))
    {
    }

    SdfGraphNodeId fromNode = 0;
    std::string fromSocket = "sdf";
    SdfGraphNodeId toNode = 0;
    std::string toSocket;
};

/// Editable graph node carrying existing SdfNode parameters/material payload.
struct SdfGraphNode {
    SdfGraphNode(SdfGraphNodeId nodeId, SdfNode nodePayload, float x = 0.0f, float y = 0.0f)
        : id(nodeId)
        , payload(std::move(nodePayload))
        , editorX(x)
        , editorY(y)
    {
    }

    SdfGraphNodeId id = 0;
    SdfNode payload;
    std::vector<SdfGraphSocket> inputs;
    std::vector<SdfGraphSocket> outputs;
    float editorX = 0.0f;
    float editorY = 0.0f;
};

/// Directed acyclic SDF graph used by future node-editor UI and compiler passes.
class SdfGraph {
public:
    SdfGraph();

    /// Creates a graph node and returns its stable ID.
    SdfGraphNodeId createNode(SdfNodeType type, std::string name = {});

    /// Deletes a node and all links connected to it.
    bool deleteNode(SdfGraphNodeId id);

    /// Connects one node output to a named input socket on another node.
    bool link(SdfGraphNodeId fromNode, SdfGraphNodeId toNode, std::string toSocket);

    /// Connects one named output socket to one named input socket.
    bool link(SdfGraphNodeId fromNode, std::string fromSocket, SdfGraphNodeId toNode, std::string toSocket);

    /// Removes all links targeting a node socket.
    bool unlinkInput(SdfGraphNodeId toNode, const std::string& toSocket);

    /// Removes one exact link between two sockets.
    bool unlink(SdfGraphNodeId fromNode, const std::string& fromSocket, SdfGraphNodeId toNode, const std::string& toSocket);

    /// Sets the graph node used as the render output.
    bool setOutputNode(SdfGraphNodeId id);

    /// Returns the graph node used as the render output, or 0 for empty graph.
    SdfGraphNodeId outputNode() const;

    /// Sets the currently selected graph node.
    bool setSelectedNode(SdfGraphNodeId id);

    /// Returns the currently selected graph node, or 0 when nothing is selected.
    SdfGraphNodeId selectedNode() const;

    /// Returns true when the node is the permanent graph output marker.
    bool isOutputNode(SdfGraphNodeId id) const;

    /// Returns true when any link starts from or ends at the node.
    bool hasLinks(SdfGraphNodeId id) const;

    /// Returns a mutable graph node by ID, or nullptr if missing.
    SdfGraphNode* node(SdfGraphNodeId id);

    /// Returns a graph node by ID, or nullptr if missing.
    const SdfGraphNode* node(SdfGraphNodeId id) const;

    /// Returns all graph nodes keyed by stable ID.
    const std::unordered_map<SdfGraphNodeId, SdfGraphNode>& nodes() const;

    /// Returns all directed graph links.
    const std::vector<SdfGraphLink>& links() const;

private:
    // AGENT: IDs are generated inside the graph instead of reusing pointer
    // identity so future serialization, undo, and visual node state stay stable.
    SdfGraphNodeId m_nextId = 1;
    SdfGraphNodeId m_outputNode = 0;
    SdfGraphNodeId m_selectedNode = 0;
    std::unordered_map<SdfGraphNodeId, SdfGraphNode> m_nodes;
    std::vector<SdfGraphLink> m_links;
};

} // namespace sdf3d
