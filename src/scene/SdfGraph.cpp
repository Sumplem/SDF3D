#include "sdf3d/scene/SdfGraph.h"

#include <algorithm>
#include <utility>

namespace sdf3d {
namespace {

SdfGraphSocket inputSocket(std::string name, SdfSocketType type = SdfSocketType::Sdf, bool multiInput = false)
{
    return {std::move(name), type, SdfSocketDirection::Input, multiInput};
}

SdfGraphSocket outputSocket(std::string name, SdfSocketType type = SdfSocketType::Sdf)
{
    return {std::move(name), type, SdfSocketDirection::Output, false};
}

std::vector<SdfGraphSocket> defaultInputsFor(SdfNodeType type)
{
    switch (type) {
    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion:
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect:
        return {inputSocket("left"), inputSocket("right")};
    case SdfNodeType::Subtract:
    case SdfNodeType::SmoothSubtract:
        return {inputSocket("base"), inputSocket("cutter")};
    case SdfNodeType::Translate:
    case SdfNodeType::Rotate:
    case SdfNodeType::Scale:
    case SdfNodeType::MaterialOverride:
        return {inputSocket("child")};
    default:
        return {};
    }
}

std::vector<SdfGraphSocket> defaultOutputsFor(SdfNodeType type)
{
    (void)type;
    return {outputSocket("sdf")};
}

const SdfGraphSocket* findSocket(const std::vector<SdfGraphSocket>& sockets, const std::string& name, SdfSocketDirection direction)
{
    for (const SdfGraphSocket& socket : sockets) {
        if (socket.name == name && socket.direction == direction) {
            return &socket;
        }
    }

    return nullptr;
}

} // namespace

SdfGraph::SdfGraph()
{
}

SdfGraphNodeId SdfGraph::createNode(SdfNodeType type, std::string name)
{
    const SdfGraphNodeId id = m_nextId++;
    SdfGraphNode graphNode{id, SdfNode(type, std::move(name)), 0.0f, 0.0f};
    graphNode.inputs = defaultInputsFor(type);
    graphNode.outputs = defaultOutputsFor(type);

    // AGENT: First node becomes output by default so a graph can render as soon
    // as it contains one valid SDF expression.
    m_nodes.emplace(id, std::move(graphNode));
    if (m_outputNode == 0) {
        m_outputNode = id;
    }
    m_selectedNode = id;
    return id;
}

bool SdfGraph::deleteNode(SdfGraphNodeId id)
{
    if (m_nodes.erase(id) == 0) {
        return false;
    }

    m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                      [id](const SdfGraphLink& link) {
                          return link.fromNode == id || link.toNode == id;
                      }),
        m_links.end());

    if (m_outputNode == id) {
        m_outputNode = 0;
    }
    if (m_selectedNode == id) {
        m_selectedNode = 0;
    }

    return true;
}

bool SdfGraph::link(SdfGraphNodeId fromNode, SdfGraphNodeId toNode, std::string toSocket)
{
    return link(fromNode, "sdf", toNode, std::move(toSocket));
}

bool SdfGraph::link(SdfGraphNodeId fromNode, std::string fromSocket, SdfGraphNodeId toNode, std::string toSocket)
{
    if (fromNode == 0 || toNode == 0 || fromNode == toNode || toSocket.empty()) {
        return false;
    }

    auto fromIt = m_nodes.find(fromNode);
    auto toIt = m_nodes.find(toNode);
    if (fromIt == m_nodes.end() || toIt == m_nodes.end()) {
        return false;
    }

    const SdfGraphSocket* output = findSocket(fromIt->second.outputs, fromSocket, SdfSocketDirection::Output);
    const SdfGraphSocket* input = findSocket(toIt->second.inputs, toSocket, SdfSocketDirection::Input);
    if (output == nullptr || input == nullptr || output->type != input->type) {
        return false;
    }

    if (!input->multiInput) {
        unlinkInput(toNode, toSocket);
    }

    // AGENT: Links now carry both sockets, matching Geometry Nodes semantics
    // while preserving the previous default `sdf` output path.
    m_links.push_back({fromNode, std::move(fromSocket), toNode, std::move(toSocket)});
    return true;
}

bool SdfGraph::unlinkInput(SdfGraphNodeId toNode, const std::string& toSocket)
{
    const size_t oldSize = m_links.size();
    m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                      [toNode, &toSocket](const SdfGraphLink& link) {
                          return link.toNode == toNode && link.toSocket == toSocket;
                      }),
        m_links.end());

    return m_links.size() != oldSize;
}

bool SdfGraph::setOutputNode(SdfGraphNodeId id)
{
    if (id != 0 && m_nodes.find(id) == m_nodes.end()) {
        return false;
    }

    m_outputNode = id;
    return true;
}

SdfGraphNodeId SdfGraph::outputNode() const
{
    return m_outputNode;
}

bool SdfGraph::setSelectedNode(SdfGraphNodeId id)
{
    if (id != 0 && m_nodes.find(id) == m_nodes.end()) {
        return false;
    }

    m_selectedNode = id;
    return true;
}

SdfGraphNodeId SdfGraph::selectedNode() const
{
    return m_selectedNode;
}

SdfGraphNode* SdfGraph::node(SdfGraphNodeId id)
{
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) {
        return nullptr;
    }

    return &it->second;
}

const SdfGraphNode* SdfGraph::node(SdfGraphNodeId id) const
{
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) {
        return nullptr;
    }

    return &it->second;
}

const std::unordered_map<SdfGraphNodeId, SdfGraphNode>& SdfGraph::nodes() const
{
    return m_nodes;
}

const std::vector<SdfGraphLink>& SdfGraph::links() const
{
    return m_links;
}

} // namespace sdf3d
