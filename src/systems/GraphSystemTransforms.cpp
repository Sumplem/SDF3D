#include "sdf3d/systems/GraphSystem.h"

#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/systems/SelectionSystem.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace sdf3d {
namespace {

int affineTransformOrder(SdfNodeType type)
{
    if (type == SdfNodeType::Scale) {
        return 0;
    }
    if (type == SdfNodeType::Rotate) {
        return 1;
    }
    if (type == SdfNodeType::Translate) {
        return 2;
    }
    return 100;
}

bool canWrapWithTransform(SdfNodeType type)
{
    return isSdfPrimitiveNode(type) || isSdfPassThroughNode(type);
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
        if (parent == nullptr || !isSdfPassThroughNode(parent->payload.type)) {
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

SdfGraphNodeId findPassThroughChildOfType(const SdfGraph& graph, SdfGraphNodeId id, SdfNodeType type)
{
    std::vector<SdfGraphNodeId> visited;
    SdfGraphNodeId currentId = id;
    while (currentId != 0 && std::find(visited.begin(), visited.end(), currentId) == visited.end()) {
        visited.push_back(currentId);
        const std::optional<SdfGraphLink> childLink = linkToInput(graph, currentId, "child");
        const std::optional<SdfGraphLink> sdfLink = childLink ? childLink : linkToInput(graph, currentId, "sdf");
        if (!sdfLink) {
            return 0;
        }

        const SdfGraphNode* child = graph.node(sdfLink->fromNode);
        if (child == nullptr || !canWrapWithTransform(child->payload.type)) {
            return 0;
        }
        if (child->payload.type == type) {
            return child->id;
        }
        if (!isSdfPassThroughNode(child->payload.type)) {
            return 0;
        }
        currentId = child->id;
    }

    return 0;
}

SdfGraphNodeId findPassThroughNodeOfTypeInChain(const SdfGraph& graph, SdfGraphNodeId id, SdfNodeType type)
{
    const SdfGraphNode* node = graph.node(id);
    if (node != nullptr && node->payload.type == type) {
        return id;
    }
    if (const SdfGraphNodeId parent = findPassThroughParentOfType(graph, id, type)) {
        return parent;
    }
    return findPassThroughChildOfType(graph, id, type);
}

SdfGraphNodeId affineChainStart(const SdfGraph& graph, SdfGraphNodeId id)
{
    SdfGraphNodeId currentId = id;
    std::vector<SdfGraphNodeId> visited;
    while (currentId != 0 && std::find(visited.begin(), visited.end(), currentId) == visited.end()) {
        visited.push_back(currentId);
        const SdfGraphNode* current = graph.node(currentId);
        if (current == nullptr || !isSdfAffineTransformNode(current->payload.type)) {
            return currentId;
        }

        const std::optional<SdfGraphLink> child = linkToInput(graph, currentId, "child");
        if (!child) {
            return currentId;
        }
        const SdfGraphNode* upstream = graph.node(child->fromNode);
        if (upstream == nullptr || (!isSdfPrimitiveNode(upstream->payload.type) && !isSdfAffineTransformNode(upstream->payload.type))) {
            return currentId;
        }
        currentId = upstream->id;
    }

    return currentId;
}

SdfGraphNodeId canonicalTransformInsertionChild(const SdfGraph& graph, SdfGraphNodeId id, SdfNodeType type)
{
    SdfGraphNodeId childId = affineChainStart(graph, id);
    SdfGraphNodeId currentId = childId;
    const int targetOrder = affineTransformOrder(type);
    std::vector<SdfGraphNodeId> visited;
    while (currentId != 0 && std::find(visited.begin(), visited.end(), currentId) == visited.end()) {
        visited.push_back(currentId);
        const SdfGraphNodeId parentId = singlePassThroughParent(graph, currentId);
        const SdfGraphNode* parent = graph.node(parentId);
        if (parent == nullptr || !isSdfAffineTransformNode(parent->payload.type)) {
            break;
        }
        if (affineTransformOrder(parent->payload.type) >= targetOrder) {
            break;
        }
        childId = parentId;
        currentId = parentId;
    }

    return childId;
}

SdfGraphNodeId insertTransformAfter(SdfGraph& graph, SdfGraphNodeId childId, SdfNodeType type, const char* name)
{
    const SdfGraphNode* child = graph.node(childId);
    if (child == nullptr) {
        return 0;
    }

    const std::vector<SdfGraphLink> links = graph.links();
    const SdfGraphNodeId transformId = GraphSystem::createNode(graph, type, name);
    SdfGraphNode* transform = graph.node(transformId);
    if (transform == nullptr) {
        return 0;
    }

    transform->editorX = child->editorX + 260.0f;
    transform->editorY = child->editorY;
    GraphSystem::link(graph, childId, "sdf", transformId, "child");

    for (const SdfGraphLink& existing : links) {
        if (existing.fromNode != childId || existing.fromSocket != "sdf") {
            continue;
        }
        GraphSystem::unlink(graph, existing.fromNode, existing.fromSocket, existing.toNode, existing.toSocket);
        GraphSystem::link(graph, transformId, "sdf", existing.toNode, existing.toSocket);
    }

    SelectionSystem::setSelectedNode(graph, transformId);
    return transformId;
}

} // namespace

SdfGraphNodeId GraphSystem::findDirectTranslateParent(const SdfGraph& graph, SdfGraphNodeId id)
{
    for (const SdfGraphLink& link : graph.links()) {
        if (link.fromNode != id || link.fromSocket != "sdf" || link.toSocket != "child") {
            continue;
        }
        const SdfGraphNode* target = graph.node(link.toNode);
        if (target != nullptr && target->payload.type == SdfNodeType::Translate) {
            return link.toNode;
        }
    }

    return 0;
}

SdfGraphNodeId GraphSystem::findDirectRotateParent(const SdfGraph& graph, SdfGraphNodeId id)
{
    for (const SdfGraphLink& link : graph.links()) {
        if (link.fromNode != id || link.fromSocket != "sdf" || link.toSocket != "child") {
            continue;
        }
        const SdfGraphNode* target = graph.node(link.toNode);
        if (target != nullptr && target->payload.type == SdfNodeType::Rotate) {
            return link.toNode;
        }
    }

    return 0;
}

SdfGraphNodeId GraphSystem::findDirectScaleParent(const SdfGraph& graph, SdfGraphNodeId id)
{
    for (const SdfGraphLink& link : graph.links()) {
        if (link.fromNode != id || link.fromSocket != "sdf" || link.toSocket != "child") {
            continue;
        }
        const SdfGraphNode* target = graph.node(link.toNode);
        if (target != nullptr && target->payload.type == SdfNodeType::Scale) {
            return link.toNode;
        }
    }

    return 0;
}

SdfGraphNodeId GraphSystem::ensureTranslateWrapperForNode(SdfGraph& graph, SdfGraphNodeId id)
{
    const SdfGraphNode* node = graph.node(id);
    if (node == nullptr || !canWrapWithTransform(node->payload.type)) {
        return 0;
    }

    if (const SdfGraphNodeId existingTranslate = findPassThroughNodeOfTypeInChain(graph, id, SdfNodeType::Translate)) {
        SelectionSystem::setSelectedNode(graph, existingTranslate);
        return existingTranslate;
    }

    const SdfGraphNodeId childId = canonicalTransformInsertionChild(graph, id, SdfNodeType::Translate);
    if (childId == 0) {
        return 0;
    }
    return insertTransformAfter(graph, childId, SdfNodeType::Translate, "Translate");
}

SdfGraphNodeId GraphSystem::ensureRotateWrapperForNode(SdfGraph& graph, SdfGraphNodeId id)
{
    const SdfGraphNode* node = graph.node(id);
    if (node == nullptr || !canWrapWithTransform(node->payload.type)) {
        return 0;
    }

    if (const SdfGraphNodeId existingRotate = findPassThroughNodeOfTypeInChain(graph, id, SdfNodeType::Rotate)) {
        SelectionSystem::setSelectedNode(graph, existingRotate);
        return existingRotate;
    }

    const SdfGraphNodeId childId = canonicalTransformInsertionChild(graph, id, SdfNodeType::Rotate);
    if (childId == 0) {
        return 0;
    }
    return insertTransformAfter(graph, childId, SdfNodeType::Rotate, "Rotate");
}

SdfGraphNodeId GraphSystem::ensureScaleWrapperForNode(SdfGraph& graph, SdfGraphNodeId id)
{
    const SdfGraphNode* node = graph.node(id);
    if (node == nullptr || !canWrapWithTransform(node->payload.type)) {
        return 0;
    }

    if (const SdfGraphNodeId existingScale = findPassThroughNodeOfTypeInChain(graph, id, SdfNodeType::Scale)) {
        SelectionSystem::setSelectedNode(graph, existingScale);
        return existingScale;
    }

    const SdfGraphNodeId childId = canonicalTransformInsertionChild(graph, id, SdfNodeType::Scale);
    if (childId == 0) {
        return 0;
    }
    return insertTransformAfter(graph, childId, SdfNodeType::Scale, "Scale");
}

glm::vec3 GraphSystem::accumulatedTranslatePosition(const SdfGraph& graph, SdfGraphNodeId translateId)
{
    glm::vec3 total = {0.0f, 0.0f, 0.0f};
    std::vector<SdfGraphNodeId> visited;
    SdfGraphNodeId currentId = translateId;

    while (currentId != 0 && std::find(visited.begin(), visited.end(), currentId) == visited.end()) {
        visited.push_back(currentId);
        const SdfGraphNode* current = graph.node(currentId);
        if (current == nullptr) {
            break;
        }

        if (current->payload.type == SdfNodeType::Translate) {
            total += translatePosition(*current);
            const std::optional<SdfGraphLink> childLink = linkToInput(graph, currentId, "child");
            if (!childLink) {
                break;
            }
            currentId = childLink->fromNode;
            continue;
        }

        if (isSdfPrimitiveNode(current->payload.type)) {
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
    SdfGraphNode* primitive = graph.node(primitiveNode);
    if (primitive == nullptr || !isSdfPrimitiveNode(primitive->payload.type)) {
        return 0;
    }

    const std::optional<SdfGraphLink> existingOutput = outputSurfaceLink(graph);
    const SdfGraphNodeId outputNode = graph.outputNode();
    if (!existingOutput) {
        link(graph, primitiveNode, "sdf", outputNode, "surface");
    } else {
        const float editorX = primitive->editorX;
        const float editorY = primitive->editorY;
        const SdfGraphNodeId unionNode = createNode(graph, SdfNodeType::Union, "Union");
        if (SdfGraphNode* node = graph.node(unionNode)) {
            node->editorX = editorX + 520.0f;
            node->editorY = editorY;
        }

        unlink(graph, existingOutput->fromNode, existingOutput->fromSocket, existingOutput->toNode, existingOutput->toSocket);
        link(graph, existingOutput->fromNode, existingOutput->fromSocket, unionNode, "inputs");
        link(graph, primitiveNode, "sdf", unionNode, "inputs");
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
