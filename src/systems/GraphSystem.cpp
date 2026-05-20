#include "sdf3d/systems/GraphSystem.h"

#include "sdf3d/core/EventBus.h"
#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/scene/SdfRotationParams.h"
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

float parameterOr(const SdfNode& node, const std::string& key, float fallback)
{
    const auto it = node.parameters.find(key);
    return it == node.parameters.end() ? fallback : it->second;
}

void clearMaterialOverrideIfMaterialInputRemoved(
    std::unordered_map<SdfGraphNodeId, SdfGraphNode>& nodes,
    SdfGraphNodeId toNode,
    const std::string& toSocket)
{
    if (toSocket != "material") {
        return;
    }

    const auto it = nodes.find(toNode);
    if (it == nodes.end() || it->second.payload.type != SdfNodeType::MaterialOverride) {
        return;
    }

    it->second.payload.materialId = 0;
    it->second.payload.material = {};
}

std::vector<SdfGraphNodeId> materialOverridesUsingDeletedSource(
    const std::unordered_map<SdfGraphNodeId, SdfGraphNode>& nodes,
    const std::vector<SdfGraphLink>& links,
    SdfGraphNodeId sourceId,
    MaterialId materialId)
{
    std::vector<SdfGraphNodeId> overrideIds;
    if (materialId == 0) {
        return overrideIds;
    }

    for (const SdfGraphLink& link : links) {
        if (link.fromNode != sourceId || link.fromSocket != "material" || link.toSocket != "material") {
            continue;
        }

        const auto it = nodes.find(link.toNode);
        if (it != nodes.end() && it->second.payload.type == SdfNodeType::MaterialOverride && it->second.payload.materialId == materialId) {
            overrideIds.push_back(link.toNode);
        }
    }

    return overrideIds;
}

bool nodeHasSdfOutput(const SdfGraphNode& node)
{
    return findSocket(node.outputs, "sdf", SdfSocketDirection::Output) != nullptr;
}

} // namespace

void GraphSystem::initialize(SdfGraph& graph)
{
    const SdfGraphNodeId id = graph.m_nextId++;
    SdfNodePtr payload = makeSdfNodeFromDefinition(SdfNodeType::Output);
    payload->stableId = id;
    SdfGraphNode graphNode{id, *payload, 560.0f, 40.0f};
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
    payload->stableId = id;
    if (type == SdfNodeType::SolidMaterial || type == SdfNodeType::CheckerMaterial) {
        payload->material.type = type == SdfNodeType::CheckerMaterial ? SdfMaterialType::Checker : SdfMaterialType::Solid;
    }
    SdfGraphNode graphNode{id, *payload, 0.0f, 0.0f};
    if (type == SdfNodeType::SolidMaterial || type == SdfNodeType::CheckerMaterial) {
        graphNode.payload.materialId = graph.m_materials.createMaterial(graphNode.payload.name.empty() ? "Material" : graphNode.payload.name, graphNode.payload.material);
    }
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
    graphNode.payload.stableId = duplicateId;
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

SdfGraphNodeId GraphSystem::groupSelection(
    SdfGraph& graph,
    GraphGroupRegistry& groups,
    const std::vector<SdfGraphNodeId>& ids,
    SdfGraphNodeId primary,
    std::string name)
{
    std::vector<SdfGraphNodeId> sourceIds;
    for (const SdfGraphNodeId id : ids) {
        if (id == 0 || isOutputNode(graph, id) || graph.m_nodes.find(id) == graph.m_nodes.end()) {
            continue;
        }
        if (std::find(sourceIds.begin(), sourceIds.end(), id) == sourceIds.end()) {
            sourceIds.push_back(id);
        }
    }
    if (sourceIds.empty()) {
        return 0;
    }

    std::unordered_set<SdfGraphNodeId> sourceSet(sourceIds.begin(), sourceIds.end());
    for (const SdfGraphLink& link : graph.m_links) {
        const bool fromSelected = sourceSet.find(link.fromNode) != sourceSet.end();
        const bool toSelected = sourceSet.find(link.toNode) != sourceSet.end();
        if (!fromSelected && toSelected) {
            return 0;
        }
    }

    SdfGraphNodeId groupRoot = 0;
    for (const SdfGraphLink& link : graph.m_links) {
        const bool fromSelected = sourceSet.find(link.fromNode) != sourceSet.end();
        const bool toSelected = sourceSet.find(link.toNode) != sourceSet.end();
        if (fromSelected && !toSelected && link.fromSocket == "sdf") {
            groupRoot = link.fromNode;
            if (link.toNode == graph.m_outputNode && link.toSocket == "surface") {
                break;
            }
        }
    }
    if (groupRoot == 0 && sourceSet.find(primary) != sourceSet.end()) {
        const auto primaryIt = graph.m_nodes.find(primary);
        if (primaryIt != graph.m_nodes.end() && nodeHasSdfOutput(primaryIt->second)) {
            groupRoot = primary;
        }
    }
    if (groupRoot == 0) {
        for (const SdfGraphNodeId id : sourceIds) {
            const auto it = graph.m_nodes.find(id);
            if (it != graph.m_nodes.end() && nodeHasSdfOutput(it->second)) {
                groupRoot = id;
                break;
            }
        }
    }
    if (groupRoot == 0) {
        return 0;
    }

    std::unordered_map<SdfGraphNodeId, SdfGraphNode> groupNodes;
    SdfGraphNodeId maxId = 0;
    float x = 0.0f;
    float y = 0.0f;
    for (const SdfGraphNodeId id : sourceIds) {
        const SdfGraphNode& node = graph.m_nodes.at(id);
        groupNodes.emplace(id, node);
        maxId = std::max(maxId, id);
        x += node.editorX;
        y += node.editorY;
    }

    const SdfGraphNodeId outputId = maxId + 1;
    SdfNodePtr outputPayload = makeSdfNodeFromDefinition(SdfNodeType::Output);
    outputPayload->stableId = outputId;
    SdfGraphNode outputNode{outputId, *outputPayload, x / static_cast<float>(sourceIds.size()) + 280.0f, y / static_cast<float>(sourceIds.size())};
    outputNode.inputs = defaultInputsFor(SdfNodeType::Output);
    outputNode.outputs = defaultOutputsFor(SdfNodeType::Output);
    groupNodes.emplace(outputId, std::move(outputNode));

    std::vector<SdfGraphLink> groupLinks;
    std::vector<SdfGraphLink> outgoingLinks;
    for (const SdfGraphLink& link : graph.m_links) {
        const bool fromSelected = sourceSet.find(link.fromNode) != sourceSet.end();
        const bool toSelected = sourceSet.find(link.toNode) != sourceSet.end();
        if (fromSelected && toSelected) {
            groupLinks.push_back(link);
        } else if (link.fromNode == groupRoot && !toSelected && link.fromSocket == "sdf") {
            outgoingLinks.push_back(link);
        }
    }
    groupLinks.push_back({groupRoot, "sdf", outputId, "surface"});

    SdfGraph subgraph;
    subgraph.m_nextId = outputId + 1;
    subgraph.m_outputNode = outputId;
    subgraph.m_selectedNode = 0;
    subgraph.m_selectedNodes = {};
    subgraph.m_nodes = std::move(groupNodes);
    subgraph.m_links = std::move(groupLinks);
    (void)subgraph.materials().replaceMaterials(
        std::vector<MaterialDefinition>{graph.materials().materials().begin(), graph.materials().materials().end()},
        graph.materials().nextMaterialIdForSerialization());

    const GroupDefId definitionId = groups.createDefinition(name.empty() ? "Group" : std::move(name), std::move(subgraph));
    for (const SdfGraphNodeId id : sourceIds) {
        (void)deleteNode(graph, id);
    }

    const SdfGraphNodeId groupId = createNode(graph, SdfNodeType::Group, "Group");
    SdfGraphNode* groupNode = graph.node(groupId);
    if (groupNode == nullptr) {
        return 0;
    }
    groupNode->payload.groupDefinitionId = definitionId;
    groupNode->editorX = x / static_cast<float>(sourceIds.size());
    groupNode->editorY = y / static_cast<float>(sourceIds.size());
    for (const SdfGraphLink& link : outgoingLinks) {
        (void)GraphSystem::link(graph, groupId, "sdf", link.toNode, link.toSocket);
    }
    SelectionSystem::setSelectedNode(graph, groupId);
    return groupId;
}

bool GraphSystem::deleteNode(SdfGraph& graph, SdfGraphNodeId id)
{
    if (isOutputNode(graph, id)) {
        return false;
    }

    MaterialId materialIdToCleanup = 0;
    const auto nodeIt = graph.m_nodes.find(id);
    if (nodeIt == graph.m_nodes.end()) {
        return false;
    }
    if (nodeIt->second.payload.type == SdfNodeType::SolidMaterial
        || nodeIt->second.payload.type == SdfNodeType::CheckerMaterial
        || nodeIt->second.payload.type == SdfNodeType::MaterialOverride) {
        materialIdToCleanup = nodeIt->second.payload.materialId;
    }
    const std::vector<SdfGraphNodeId> linkedOverridesToClear = isSdfMaterialNode(nodeIt->second.payload.type)
        ? materialOverridesUsingDeletedSource(graph.m_nodes, graph.m_links, id, materialIdToCleanup)
        : std::vector<SdfGraphNodeId>{};

    graph.m_nodes.erase(nodeIt);

    for (const SdfGraphNodeId overrideId : linkedOverridesToClear) {
        const auto overrideIt = graph.m_nodes.find(overrideId);
        if (overrideIt != graph.m_nodes.end() && overrideIt->second.payload.materialId == materialIdToCleanup) {
            overrideIt->second.payload.materialId = 0;
            overrideIt->second.payload.material = {};
        }
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
    if (materialIdToCleanup != 0 && canDeleteMaterial(graph, materialIdToCleanup)) {
        (void)graph.m_materials.removeMaterial(materialIdToCleanup);
    }
    return true;
}

bool GraphSystem::renameMaterial(SdfGraph& graph, MaterialId id, std::string name)
{
    MaterialDefinition* material = graph.m_materials.material(id);
    if (material == nullptr || name.empty()) {
        return false;
    }

    material->name = std::move(name);
    for (auto& [nodeId, node] : graph.m_nodes) {
        (void)nodeId;
        if (node.payload.materialId == id) {
            node.payload.name = material->name;
        }
    }
    return true;
}

bool GraphSystem::canDeleteMaterial(const SdfGraph& graph, MaterialId id)
{
    if (id == 0 || graph.m_materials.material(id) == nullptr) {
        return false;
    }

    for (const auto& [nodeId, node] : graph.m_nodes) {
        (void)nodeId;
        if (node.payload.materialId == id) {
            return false;
        }
    }
    return true;
}

bool GraphSystem::deleteMaterial(SdfGraph& graph, MaterialId id)
{
    if (!canDeleteMaterial(graph, id)) {
        return false;
    }

    return graph.m_materials.removeMaterial(id);
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
    if (toIt->second.payload.type == SdfNodeType::MaterialOverride && graph.m_links.back().toSocket == "material") {
        toIt->second.payload.materialId = fromIt->second.payload.materialId;
        if (const MaterialDefinition* material = graph.m_materials.material(fromIt->second.payload.materialId)) {
            toIt->second.payload.material = material->material;
        }
    }
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

    if (graph.m_links.size() != oldSize) {
        clearMaterialOverrideIfMaterialInputRemoved(graph.m_nodes, toNode, toSocket);
    }
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

    if (graph.m_links.size() != oldSize) {
        clearMaterialOverrideIfMaterialInputRemoved(graph.m_nodes, toNode, toSocket);
    }
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

std::vector<SdfCompiledNodeParam> GraphSystem::collectNodeParams(const SdfGraph& graph)
{
    std::vector<SdfCompiledNodeParam> params;
    params.reserve(graph.nodes().size());
    for (const auto& [id, node] : graph.nodes()) {
        SdfCompiledNodeParam param;
        param.nodeId = id;
        switch (node.payload.type) {
        case SdfNodeType::Translate:
            param.data0 = {
                parameterOr(node.payload, "x", 0.0f),
                parameterOr(node.payload, "y", 0.0f),
                parameterOr(node.payload, "z", 0.0f),
                0.0f,
            };
            params.push_back(param);
            break;
        case SdfNodeType::Rotate:
        {
            const glm::vec4 q = rotationQuaternionForNode(node.payload);
            param.data0 = {
                q.x,
                q.y,
                q.z,
                q.w,
            };
            params.push_back(param);
            break;
        }
        case SdfNodeType::Scale: {
            const float uniformScale = parameterOr(node.payload, "scale", 1.0f);
            const float x = std::max(parameterOr(node.payload, "x", uniformScale), 0.0001f);
            const float y = std::max(parameterOr(node.payload, "y", uniformScale), 0.0001f);
            const float z = std::max(parameterOr(node.payload, "z", uniformScale), 0.0001f);
            param.data0 = {x, y, z, std::min({x, y, z})};
            params.push_back(param);
            break;
        }
        default:
            break;
        }
    }

    return params;
}

} // namespace sdf3d
