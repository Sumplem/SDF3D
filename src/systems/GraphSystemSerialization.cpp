#include "sdf3d/systems/GraphSystem.h"

#include <algorithm>
#include <utility>

namespace sdf3d {
namespace {

const SdfGraphSocket* findSocket(const std::vector<SdfGraphSocket>& sockets, const std::string& name, SdfSocketDirection direction)
{
    for (const SdfGraphSocket& socket : sockets) {
        if (socket.name == name && socket.direction == direction) {
            return &socket;
        }
    }

    return nullptr;
}

bool linkValidForNodes(const std::unordered_map<SdfGraphNodeId, SdfGraphNode>& nodes, const SdfGraphLink& link)
{
    if (link.fromNode == 0 || link.toNode == 0 || link.fromNode == link.toNode || link.toSocket.empty()) {
        return false;
    }

    const auto fromIt = nodes.find(link.fromNode);
    const auto toIt = nodes.find(link.toNode);
    if (fromIt == nodes.end() || toIt == nodes.end()) {
        return false;
    }

    const SdfGraphSocket* output = findSocket(fromIt->second.outputs, link.fromSocket, SdfSocketDirection::Output);
    const SdfGraphSocket* input = findSocket(toIt->second.inputs, link.toSocket, SdfSocketDirection::Input);
    return output != nullptr && input != nullptr && output->type == input->type;
}

bool targetInputIsMulti(const std::unordered_map<SdfGraphNodeId, SdfGraphNode>& nodes, const SdfGraphLink& link)
{
    const auto toIt = nodes.find(link.toNode);
    if (toIt == nodes.end()) {
        return false;
    }

    const SdfGraphSocket* input = findSocket(toIt->second.inputs, link.toSocket, SdfSocketDirection::Input);
    return input != nullptr && input->multiInput;
}

bool linksRespectInputCardinality(const std::unordered_map<SdfGraphNodeId, SdfGraphNode>& nodes, const std::vector<SdfGraphLink>& links)
{
    for (std::size_t i = 0; i < links.size(); ++i) {
        for (std::size_t j = i + 1; j < links.size(); ++j) {
            const bool sameTarget = links[i].toNode == links[j].toNode && links[i].toSocket == links[j].toSocket;
            if (!sameTarget) {
                continue;
            }

            const bool sameExactLink = links[i].fromNode == links[j].fromNode && links[i].fromSocket == links[j].fromSocket;
            if (sameExactLink || !targetInputIsMulti(nodes, links[i])) {
                return false;
            }
        }
    }

    return true;
}

} // namespace

bool GraphSystem::replaceGraphData(
    SdfGraph& graph,
    SdfGraphNodeId nextId,
    SdfGraphNodeId outputNode,
    SdfGraphNodeId selectedNode,
    std::vector<SdfGraphNodeId> selectedNodes,
    std::unordered_map<SdfGraphNodeId, SdfGraphNode> nodes,
    std::vector<SdfGraphLink> links)
{
    if (nextId == 0 || outputNode == 0 || nodes.empty()) {
        return false;
    }

    SdfGraphNodeId maxId = 0;
    size_t outputCount = 0;
    for (const auto& [id, node] : nodes) {
        if (id == 0 || node.id != id) {
            return false;
        }
        maxId = std::max(maxId, id);
        if (node.payload.type == SdfNodeType::Output) {
            ++outputCount;
        }
    }

    const auto outputIt = nodes.find(outputNode);
    if (outputCount != 1 || outputIt == nodes.end() || outputIt->second.payload.type != SdfNodeType::Output || nextId <= maxId) {
        return false;
    }

    if (selectedNode != 0 && nodes.find(selectedNode) == nodes.end()) {
        return false;
    }
    for (const SdfGraphNodeId id : selectedNodes) {
        if (id == 0 || nodes.find(id) == nodes.end()) {
            return false;
        }
    }
    if (selectedNode != 0 && std::find(selectedNodes.begin(), selectedNodes.end(), selectedNode) == selectedNodes.end()) {
        return false;
    }

    for (const SdfGraphLink& linkToValidate : links) {
        if (!linkValidForNodes(nodes, linkToValidate)) {
            return false;
        }
    }
    if (!linksRespectInputCardinality(nodes, links)) {
        return false;
    }

    graph.m_nextId = nextId;
    graph.m_outputNode = outputNode;
    graph.m_selectedNode = selectedNode;
    graph.m_selectedNodes = std::move(selectedNodes);
    graph.m_nodes = std::move(nodes);
    graph.m_links = std::move(links);
    return true;
}

} // namespace sdf3d
