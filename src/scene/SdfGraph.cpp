#include "sdf3d/scene/SdfGraph.h"

#include "sdf3d/scene/SdfNodeDefinition.h"

#include <algorithm>
#include <utility>

namespace sdf3d {
namespace {

std::vector<SdfGraphSocket> defaultInputsFor(SdfNodeType type)
{
    if (const SdfNodeDefinition* definition = sdfNodeDefinition(type)) {
        return definition->inputs;
    }

    return {};
}

std::vector<SdfGraphSocket> defaultOutputsFor(SdfNodeType type)
{
    if (const SdfNodeDefinition* definition = sdfNodeDefinition(type)) {
        return definition->outputs;
    }

    return {};
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
    const SdfGraphNodeId id = m_nextId++;
    SdfGraphNode graphNode{id, *makeSdfNodeFromDefinition(SdfNodeType::Output), 560.0f, 40.0f};
    graphNode.inputs = defaultInputsFor(SdfNodeType::Output);
    graphNode.outputs = defaultOutputsFor(SdfNodeType::Output);
    m_nodes.emplace(id, std::move(graphNode));
    m_outputNode = id;
    m_selectedNode = id;
}

SdfGraphNodeId SdfGraph::createNode(SdfNodeType type, std::string name)
{
    const SdfGraphNodeId id = m_nextId++;
    SdfNodePtr payload = makeSdfNodeFromDefinition(type);
    if (!name.empty()) {
        payload->name = std::move(name);
    }
    SdfGraphNode graphNode{id, *payload, 0.0f, 0.0f};
    graphNode.inputs = defaultInputsFor(type);
    graphNode.outputs = defaultOutputsFor(type);

    m_nodes.emplace(id, std::move(graphNode));
    if (m_outputNode == 0 || type == SdfNodeType::Output) {
        m_outputNode = id;
    }
    m_selectedNode = id;
    return id;
}

bool SdfGraph::deleteNode(SdfGraphNodeId id)
{
    if (isOutputNode(id)) {
        return false;
    }

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

bool SdfGraph::unlink(SdfGraphNodeId fromNode, const std::string& fromSocket, SdfGraphNodeId toNode, const std::string& toSocket)
{
    const size_t oldSize = m_links.size();
    m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                      [fromNode, &fromSocket, toNode, &toSocket](const SdfGraphLink& link) {
                          return link.fromNode == fromNode
                              && link.fromSocket == fromSocket
                              && link.toNode == toNode
                              && link.toSocket == toSocket;
                      }),
        m_links.end());

    return m_links.size() != oldSize;
}

bool SdfGraph::setOutputNode(SdfGraphNodeId id)
{
    const auto it = m_nodes.find(id);
    if (it == m_nodes.end() || it->second.payload.type != SdfNodeType::Output) {
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

bool SdfGraph::isOutputNode(SdfGraphNodeId id) const
{
    return id != 0 && id == m_outputNode;
}

bool SdfGraph::hasLinks(SdfGraphNodeId id) const
{
    for (const SdfGraphLink& link : m_links) {
        if (link.fromNode == id || link.toNode == id) {
            return true;
        }
    }

    return false;
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
