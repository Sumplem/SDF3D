#include "sdf3d/systems/GraphSystem.h"

#include "sdf3d/core/EventBus.h"
#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/systems/SelectionSystem.h"

#include <algorithm>
#include <optional>
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

bool isPrimitiveNode(SdfNodeType type)
{
    return type == SdfNodeType::Sphere
        || type == SdfNodeType::Box
        || type == SdfNodeType::Cylinder
        || type == SdfNodeType::Torus
        || type == SdfNodeType::Plane
        || type == SdfNodeType::Capsule
        || type == SdfNodeType::Cone
        || type == SdfNodeType::RoundBox;
}

bool isTransformPassThroughNode(SdfNodeType type)
{
    return type == SdfNodeType::Translate
        || type == SdfNodeType::Rotate
        || type == SdfNodeType::Scale
        || type == SdfNodeType::Repeat
        || type == SdfNodeType::Mirror
        || type == SdfNodeType::Twist
        || type == SdfNodeType::Bend
        || type == SdfNodeType::MaterialOverride;
}

bool canWrapWithTransform(SdfNodeType type)
{
    return isPrimitiveNode(type) || isTransformPassThroughNode(type);
}

float parameterOr(const SdfNode& node, const std::string& key, float fallback)
{
    const auto it = node.parameters.find(key);
    return it == node.parameters.end() ? fallback : it->second;
}

glm::vec3 translatePosition(const SdfGraphNode& node)
{
    return {
        parameterOr(node.payload, "x", 0.0f),
        parameterOr(node.payload, "y", 0.0f),
        parameterOr(node.payload, "z", 0.0f),
    };
}

std::optional<SdfGraphLink> linkToInput(const SdfGraph& graph, SdfGraphNodeId node, const std::string& socket)
{
    for (const SdfGraphLink& link : graph.links()) {
        if (link.toNode == node && link.toSocket == socket) {
            return link;
        }
    }

    return std::nullopt;
}

std::optional<SdfGraphLink> outputSurfaceLink(const SdfGraph& graph)
{
    for (const SdfGraphLink& link : graph.links()) {
        if (link.toNode == graph.outputNode() && link.toSocket == "surface") {
            return link;
        }
    }

    return std::nullopt;
}

std::optional<SdfGraphLink> singleIncomingSdfLink(const SdfGraph& graph, SdfGraphNodeId node)
{
    std::optional<SdfGraphLink> result;
    for (const SdfGraphLink& link : graph.links()) {
        if (link.toNode != node) {
            continue;
        }

        const bool fromSdf = link.fromSocket == "sdf";
        const bool toSdfInput = link.toSocket == "child" || link.toSocket == "sdf";
        if (!fromSdf || !toSdfInput) {
            continue;
        }

        if (result) {
            return std::nullopt;
        }
        result = link;
    }

    return result;
}

SdfGraphNodeId singlePassThroughParent(const SdfGraph& graph, SdfGraphNodeId id)
{
    SdfGraphNodeId parentId = 0;
    for (const SdfGraphLink& link : graph.links()) {
        if (link.fromNode != id || link.fromSocket != "sdf" || (link.toSocket != "child" && link.toSocket != "sdf")) {
            continue;
        }

        const SdfGraphNode* parent = graph.node(link.toNode);
        if (parent == nullptr || !isTransformPassThroughNode(parent->payload.type)) {
            continue;
        }
        if (parentId != 0) {
            return 0;
        }
        parentId = link.toNode;
    }
    return parentId;
}

SdfGraphNodeId findPassThroughParentOfType(const SdfGraph& graph, SdfGraphNodeId id, SdfNodeType type)
{
    std::vector<SdfGraphNodeId> visited;
    SdfGraphNodeId currentId = id;
    while (currentId != 0 && std::find(visited.begin(), visited.end(), currentId) == visited.end()) {
        visited.push_back(currentId);
        const SdfGraphNodeId parentId = singlePassThroughParent(graph, currentId);
        if (parentId == 0) {
            return 0;
        }
        const SdfGraphNode* parent = graph.node(parentId);
        if (parent != nullptr && parent->payload.type == type) {
            return parentId;
        }
        currentId = parentId;
    }
    return 0;
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

SdfGraphNodeId GraphSystem::findDirectTranslateParent(const SdfGraph& graph, SdfGraphNodeId id)
{
    for (const SdfGraphLink& link : graph.m_links) {
        if (link.fromNode != id || link.fromSocket != "sdf" || link.toSocket != "child") {
            continue;
        }
        const auto targetIt = graph.m_nodes.find(link.toNode);
        if (targetIt != graph.m_nodes.end() && targetIt->second.payload.type == SdfNodeType::Translate) {
            return link.toNode;
        }
    }

    return 0;
}

SdfGraphNodeId GraphSystem::findDirectRotateParent(const SdfGraph& graph, SdfGraphNodeId id)
{
    for (const SdfGraphLink& link : graph.m_links) {
        if (link.fromNode != id || link.fromSocket != "sdf" || link.toSocket != "child") {
            continue;
        }
        const auto targetIt = graph.m_nodes.find(link.toNode);
        if (targetIt != graph.m_nodes.end() && targetIt->second.payload.type == SdfNodeType::Rotate) {
            return link.toNode;
        }
    }

    return 0;
}

SdfGraphNodeId GraphSystem::findDirectScaleParent(const SdfGraph& graph, SdfGraphNodeId id)
{
    for (const SdfGraphLink& link : graph.m_links) {
        if (link.fromNode != id || link.fromSocket != "sdf" || link.toSocket != "child") {
            continue;
        }
        const auto targetIt = graph.m_nodes.find(link.toNode);
        if (targetIt != graph.m_nodes.end() && targetIt->second.payload.type == SdfNodeType::Scale) {
            return link.toNode;
        }
    }

    return 0;
}

SdfGraphNodeId GraphSystem::ensureTranslateWrapperForNode(SdfGraph& graph, SdfGraphNodeId id)
{
    const auto nodeIt = graph.m_nodes.find(id);
    if (nodeIt == graph.m_nodes.end()) {
        return 0;
    }
    if (nodeIt->second.payload.type == SdfNodeType::Translate) {
        SelectionSystem::setSelectedNode(graph, id);
        return id;
    }
    if (!canWrapWithTransform(nodeIt->second.payload.type)) {
        return 0;
    }

    if (const SdfGraphNodeId translateParent = findPassThroughParentOfType(graph, id, SdfNodeType::Translate)) {
        SelectionSystem::setSelectedNode(graph, translateParent);
        return translateParent;
    }

    const std::vector<SdfGraphLink> links = graph.m_links;
    const float editorX = nodeIt->second.editorX;
    const float editorY = nodeIt->second.editorY;
    const SdfGraphNodeId translateId = createNode(graph, SdfNodeType::Translate, "Translate");
    auto translateIt = graph.m_nodes.find(translateId);
    if (translateIt == graph.m_nodes.end()) {
        return 0;
    }

    translateIt->second.editorX = editorX + 260.0f;
    translateIt->second.editorY = editorY;
    link(graph, id, "sdf", translateId, "child");

    for (const SdfGraphLink& existing : links) {
        if (existing.fromNode != id || existing.fromSocket != "sdf") {
            continue;
        }
        unlink(graph, existing.fromNode, existing.fromSocket, existing.toNode, existing.toSocket);
        link(graph, translateId, "sdf", existing.toNode, existing.toSocket);
    }

    SelectionSystem::setSelectedNode(graph, translateId);
    return translateId;
}

SdfGraphNodeId GraphSystem::ensureRotateWrapperForNode(SdfGraph& graph, SdfGraphNodeId id)
{
    const auto nodeIt = graph.m_nodes.find(id);
    if (nodeIt == graph.m_nodes.end()) {
        return 0;
    }
    if (nodeIt->second.payload.type == SdfNodeType::Rotate) {
        SelectionSystem::setSelectedNode(graph, id);
        return id;
    }
    if (!canWrapWithTransform(nodeIt->second.payload.type)) {
        return 0;
    }

    if (const SdfGraphNodeId rotateParent = findPassThroughParentOfType(graph, id, SdfNodeType::Rotate)) {
        SelectionSystem::setSelectedNode(graph, rotateParent);
        return rotateParent;
    }

    const std::vector<SdfGraphLink> links = graph.m_links;
    const float editorX = nodeIt->second.editorX;
    const float editorY = nodeIt->second.editorY;
    const SdfGraphNodeId rotateId = createNode(graph, SdfNodeType::Rotate, "Rotate");
    auto rotateIt = graph.m_nodes.find(rotateId);
    if (rotateIt == graph.m_nodes.end()) {
        return 0;
    }

    rotateIt->second.editorX = editorX + 260.0f;
    rotateIt->second.editorY = editorY;
    link(graph, id, "sdf", rotateId, "child");

    for (const SdfGraphLink& existing : links) {
        if (existing.fromNode != id || existing.fromSocket != "sdf") {
            continue;
        }
        unlink(graph, existing.fromNode, existing.fromSocket, existing.toNode, existing.toSocket);
        link(graph, rotateId, "sdf", existing.toNode, existing.toSocket);
    }

    SelectionSystem::setSelectedNode(graph, rotateId);
    return rotateId;
}

SdfGraphNodeId GraphSystem::ensureScaleWrapperForNode(SdfGraph& graph, SdfGraphNodeId id)
{
    const auto nodeIt = graph.m_nodes.find(id);
    if (nodeIt == graph.m_nodes.end()) {
        return 0;
    }
    if (nodeIt->second.payload.type == SdfNodeType::Scale) {
        SelectionSystem::setSelectedNode(graph, id);
        return id;
    }
    if (!canWrapWithTransform(nodeIt->second.payload.type)) {
        return 0;
    }

    if (const SdfGraphNodeId scaleParent = findPassThroughParentOfType(graph, id, SdfNodeType::Scale)) {
        SelectionSystem::setSelectedNode(graph, scaleParent);
        return scaleParent;
    }

    const std::vector<SdfGraphLink> links = graph.m_links;
    const float editorX = nodeIt->second.editorX;
    const float editorY = nodeIt->second.editorY;
    const SdfGraphNodeId scaleId = createNode(graph, SdfNodeType::Scale, "Scale");
    auto scaleIt = graph.m_nodes.find(scaleId);
    if (scaleIt == graph.m_nodes.end()) {
        return 0;
    }

    scaleIt->second.editorX = editorX + 260.0f;
    scaleIt->second.editorY = editorY;
    link(graph, id, "sdf", scaleId, "child");

    for (const SdfGraphLink& existing : links) {
        if (existing.fromNode != id || existing.fromSocket != "sdf") {
            continue;
        }
        unlink(graph, existing.fromNode, existing.fromSocket, existing.toNode, existing.toSocket);
        link(graph, scaleId, "sdf", existing.toNode, existing.toSocket);
    }

    SelectionSystem::setSelectedNode(graph, scaleId);
    return scaleId;
}

glm::vec3 GraphSystem::accumulatedTranslatePosition(const SdfGraph& graph, SdfGraphNodeId translateId)
{
    glm::vec3 total = {0.0f, 0.0f, 0.0f};
    std::vector<SdfGraphNodeId> visited;
    SdfGraphNodeId currentId = translateId;

    while (currentId != 0 && std::find(visited.begin(), visited.end(), currentId) == visited.end()) {
        visited.push_back(currentId);
        const auto currentIt = graph.m_nodes.find(currentId);
        if (currentIt == graph.m_nodes.end()) {
            break;
        }

        const SdfGraphNode& current = currentIt->second;
        if (current.payload.type == SdfNodeType::Translate) {
            total += translatePosition(current);
            const std::optional<SdfGraphLink> childLink = linkToInput(graph, currentId, "child");
            if (!childLink) {
                break;
            }
            currentId = childLink->fromNode;
            continue;
        }

        if (isPrimitiveNode(current.payload.type)) {
            break;
        }

        const std::optional<SdfGraphLink> upstream = singleIncomingSdfLink(graph, currentId);
        if (!upstream) {
            break;
        }
        currentId = upstream->fromNode;
    }

    return total;
}

SdfGraphNodeId GraphSystem::placePrimitiveAtWorldPosition(SdfGraph& graph, SdfGraphNodeId primitiveNode, glm::vec3 worldPosition)
{
    const auto primitiveIt = graph.m_nodes.find(primitiveNode);
    if (primitiveIt == graph.m_nodes.end() || !isPrimitiveNode(primitiveIt->second.payload.type)) {
        return 0;
    }

    const std::optional<SdfGraphLink> existingOutput = outputSurfaceLink(graph);
    const SdfGraphNodeId outputNode = graph.outputNode();
    if (!existingOutput) {
        link(graph, primitiveNode, "sdf", outputNode, "surface");
    } else {
        const float editorX = primitiveIt->second.editorX;
        const float editorY = primitiveIt->second.editorY;
        const SdfGraphNodeId unionNode = createNode(graph, SdfNodeType::Union, "Union");
        if (auto unionIt = graph.m_nodes.find(unionNode); unionIt != graph.m_nodes.end()) {
            unionIt->second.editorX = editorX + 520.0f;
            unionIt->second.editorY = editorY;
        }

        unlink(graph, existingOutput->fromNode, existingOutput->fromSocket, existingOutput->toNode, existingOutput->toSocket);
        link(graph, existingOutput->fromNode, existingOutput->fromSocket, unionNode, "left");
        link(graph, primitiveNode, "sdf", unionNode, "right");
        link(graph, unionNode, "sdf", outputNode, "surface");
    }

    const SdfGraphNodeId translateId = ensureTranslateWrapperForNode(graph, primitiveNode);
    if (SdfGraphNode* translate = graph.node(translateId)) {
        translate->payload.parameters["x"] = worldPosition.x;
        translate->payload.parameters["y"] = worldPosition.y;
        translate->payload.parameters["z"] = worldPosition.z;
    }
    SelectionSystem::setSelectedNode(graph, translateId);
    return translateId;
}

} // namespace sdf3d
