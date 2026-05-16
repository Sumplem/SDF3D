#include "sdf3d/systems/GraphSystem.h"

#include "sdf3d/core/EventBus.h"
#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/systems/SelectionSystem.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
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

void GraphSystem::initialize(SdfGraph& graph)
{
    const SdfGraphNodeId id = graph.m_nextId++;
    SdfGraphNode graphNode{id, *makeSdfNodeFromDefinition(SdfNodeType::Output), 560.0f, 40.0f};
    graphNode.inputs = defaultInputsFor(SdfNodeType::Output);
    graphNode.outputs = defaultOutputsFor(SdfNodeType::Output);
    graph.m_nodes.emplace(id, std::move(graphNode));
    graph.m_outputNode = id;
}

SdfGraphNodeId GraphSystem::createNode(SdfGraph& graph, SdfNodeType type, std::string name)
{
    const SdfGraphNodeId id = graph.m_nextId++;
    SdfNodePtr payload = makeSdfNodeFromDefinition(type);
    if (!name.empty()) {
        payload->name = std::move(name);
    }
    SdfGraphNode graphNode{id, *payload, 0.0f, 0.0f};
    graphNode.inputs = defaultInputsFor(type);
    graphNode.outputs = defaultOutputsFor(type);

    graph.m_nodes.emplace(id, std::move(graphNode));
    if (graph.m_outputNode == 0 || type == SdfNodeType::Output) {
        graph.m_outputNode = id;
    }
    return id;
}

SdfGraphNodeId GraphSystem::duplicateNode(SdfGraph& graph, SdfGraphNodeId id)
{
    if (isOutputNode(graph, id)) {
        return 0;
    }

    const auto it = graph.m_nodes.find(id);
    if (it == graph.m_nodes.end()) {
        return 0;
    }

    const SdfGraphNodeId duplicateId = graph.m_nextId++;
    SdfGraphNode graphNode{duplicateId, it->second.payload, it->second.editorX + 32.0f, it->second.editorY + 32.0f};
    graphNode.inputs = it->second.inputs;
    graphNode.outputs = it->second.outputs;
    graphNode.editorPropertiesCollapsed = it->second.editorPropertiesCollapsed;
    graph.m_nodes.emplace(duplicateId, std::move(graphNode));
    return duplicateId;
}

std::vector<SdfGraphNodeId> GraphSystem::duplicateSelection(SdfGraph& graph, const std::vector<SdfGraphNodeId>& ids, EventBus& eventBus)
{
    std::vector<SdfGraphNodeId> sourceIds;
    sourceIds.reserve(ids.size());
    for (const SdfGraphNodeId id : ids) {
        if (id == 0 || isOutputNode(graph, id) || graph.m_nodes.find(id) == graph.m_nodes.end()) {
            continue;
        }
        if (std::find(sourceIds.begin(), sourceIds.end(), id) == sourceIds.end()) {
            sourceIds.push_back(id);
        }
    }

    std::unordered_set<SdfGraphNodeId> sourceSet;
    sourceSet.reserve(sourceIds.size());
    for (const SdfGraphNodeId id : sourceIds) {
        sourceSet.insert(id);
    }

    std::unordered_map<SdfGraphNodeId, SdfGraphNodeId> duplicateIdsBySource;
    duplicateIdsBySource.reserve(sourceIds.size());
    std::vector<SdfGraphNodeId> duplicateIds;
    duplicateIds.reserve(sourceIds.size());
    for (const SdfGraphNodeId id : sourceIds) {
        const SdfGraphNodeId duplicateId = duplicateNode(graph, id);
        if (duplicateId == 0) {
            continue;
        }
        duplicateIdsBySource.emplace(id, duplicateId);
        duplicateIds.push_back(duplicateId);
    }

    if (duplicateIds.empty()) {
        return {};
    }

    const std::vector<SdfGraphLink> originalLinks = graph.m_links;
    for (const SdfGraphLink& linkToCopy : originalLinks) {
        if (sourceSet.find(linkToCopy.fromNode) == sourceSet.end()
            || sourceSet.find(linkToCopy.toNode) == sourceSet.end()) {
            continue;
        }

        const auto fromIt = duplicateIdsBySource.find(linkToCopy.fromNode);
        const auto toIt = duplicateIdsBySource.find(linkToCopy.toNode);
        if (fromIt == duplicateIdsBySource.end() || toIt == duplicateIdsBySource.end()) {
            continue;
        }

        (void)link(graph, fromIt->second, linkToCopy.fromSocket, toIt->second, linkToCopy.toSocket);
    }

    const SdfGraphNodeId primary = duplicateIds.back();
    SelectionSystem::setSelectedNodes(graph, duplicateIds, primary);
    eventBus.emit(SelectionEvent{duplicateIds, primary});
    eventBus.emit(SceneDirtyEvent{});
    return duplicateIds;
}

bool GraphSystem::deleteNode(SdfGraph& graph, SdfGraphNodeId id)
{
    if (isOutputNode(graph, id)) {
        return false;
    }

    if (graph.m_nodes.erase(id) == 0) {
        return false;
    }

    graph.m_links.erase(std::remove_if(graph.m_links.begin(), graph.m_links.end(),
                            [id](const SdfGraphLink& link) {
                                return link.fromNode == id || link.toNode == id;
                            }),
        graph.m_links.end());

    if (graph.m_outputNode == id) {
        graph.m_outputNode = 0;
    }
    graph.m_selectedNodes.erase(std::remove(graph.m_selectedNodes.begin(), graph.m_selectedNodes.end(), id), graph.m_selectedNodes.end());
    if (graph.m_selectedNode == id) {
        graph.m_selectedNode = graph.m_selectedNodes.empty() ? 0 : graph.m_selectedNodes.back();
    }
    return true;
}

bool GraphSystem::link(SdfGraph& graph, SdfGraphNodeId fromNode, SdfGraphNodeId toNode, std::string toSocket)
{
    return link(graph, fromNode, "sdf", toNode, std::move(toSocket));
}

bool GraphSystem::link(SdfGraph& graph, SdfGraphNodeId fromNode, std::string fromSocket, SdfGraphNodeId toNode, std::string toSocket)
{
    if (fromNode == 0 || toNode == 0 || fromNode == toNode || toSocket.empty()) {
        return false;
    }

    auto fromIt = graph.m_nodes.find(fromNode);
    auto toIt = graph.m_nodes.find(toNode);
    if (fromIt == graph.m_nodes.end() || toIt == graph.m_nodes.end()) {
        return false;
    }

    const SdfGraphSocket* output = findSocket(fromIt->second.outputs, fromSocket, SdfSocketDirection::Output);
    const SdfGraphSocket* input = findSocket(toIt->second.inputs, toSocket, SdfSocketDirection::Input);
    if (output == nullptr || input == nullptr || output->type != input->type) {
        return false;
    }

    if (!input->multiInput) {
        unlinkInput(graph, toNode, toSocket);
    }

    // AGENT: Links carry both sockets, matching Geometry Nodes semantics
    // while preserving default `sdf` output path.
    graph.m_links.push_back({fromNode, std::move(fromSocket), toNode, std::move(toSocket)});
    return true;
}

bool GraphSystem::unlinkInput(SdfGraph& graph, SdfGraphNodeId toNode, const std::string& toSocket)
{
    const size_t oldSize = graph.m_links.size();
    graph.m_links.erase(std::remove_if(graph.m_links.begin(), graph.m_links.end(),
                            [toNode, &toSocket](const SdfGraphLink& link) {
                                return link.toNode == toNode && link.toSocket == toSocket;
                            }),
        graph.m_links.end());

    return graph.m_links.size() != oldSize;
}

bool GraphSystem::unlink(SdfGraph& graph, SdfGraphNodeId fromNode, const std::string& fromSocket, SdfGraphNodeId toNode, const std::string& toSocket)
{
    const size_t oldSize = graph.m_links.size();
    graph.m_links.erase(std::remove_if(graph.m_links.begin(), graph.m_links.end(),
                            [fromNode, &fromSocket, toNode, &toSocket](const SdfGraphLink& link) {
                                return link.fromNode == fromNode
                                    && link.fromSocket == fromSocket
                                    && link.toNode == toNode
                                    && link.toSocket == toSocket;
                            }),
        graph.m_links.end());

    return graph.m_links.size() != oldSize;
}

bool GraphSystem::setOutputNode(SdfGraph& graph, SdfGraphNodeId id)
{
    const auto it = graph.m_nodes.find(id);
    if (it == graph.m_nodes.end() || it->second.payload.type != SdfNodeType::Output) {
        return false;
    }

    graph.m_outputNode = id;
    return true;
}

bool GraphSystem::isOutputNode(const SdfGraph& graph, SdfGraphNodeId id)
{
    return id != 0 && id == graph.m_outputNode;
}

bool GraphSystem::hasLinks(const SdfGraph& graph, SdfGraphNodeId id)
{
    for (const SdfGraphLink& link : graph.m_links) {
        if (link.fromNode == id || link.toNode == id) {
            return true;
        }
    }

    return false;
}

} // namespace sdf3d
