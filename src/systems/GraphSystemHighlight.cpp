#include "sdf3d/systems/GraphSystem.h"

#include <algorithm>
#include <vector>

namespace sdf3d {
namespace {

bool isHighlightPassThrough(SdfNodeType type)
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

bool isHighlightTransform(SdfNodeType type)
{
    return type == SdfNodeType::Translate
        || type == SdfNodeType::Rotate
        || type == SdfNodeType::Scale
        || type == SdfNodeType::Repeat
        || type == SdfNodeType::Mirror
        || type == SdfNodeType::Twist
        || type == SdfNodeType::Bend;
}

bool isBooleanNode(SdfNodeType type)
{
    return type == SdfNodeType::Union
        || type == SdfNodeType::SmoothUnion
        || type == SdfNodeType::Subtract
        || type == SdfNodeType::SmoothSubtract
        || type == SdfNodeType::Intersect
        || type == SdfNodeType::SmoothIntersect;
}

bool feedsBooleanNode(const SdfGraph& graph, SdfGraphNodeId id)
{
    for (const SdfGraphLink& link : graph.links()) {
        if (link.fromNode != id || link.fromSocket != "sdf") {
            continue;
        }

        const SdfGraphNode* parent = graph.node(link.toNode);
        if (parent != nullptr && isBooleanNode(parent->payload.type)) {
            return true;
        }
    }

    return false;
}

SdfGraphNodeId singlePassThroughParent(const SdfGraph& graph, SdfGraphNodeId id)
{
    if (feedsBooleanNode(graph, id)) {
        return 0;
    }

    SdfGraphNodeId parentId = 0;
    for (const SdfGraphLink& link : graph.links()) {
        if (link.fromNode != id || link.fromSocket != "sdf") {
            continue;
        }

        if (link.toSocket != "child" && link.toSocket != "sdf") {
            continue;
        }

        const SdfGraphNode* parent = graph.node(link.toNode);
        if (parent == nullptr || !isHighlightPassThrough(parent->payload.type)) {
            continue;
        }
        if (parentId != 0) {
            return 0;
        }
        parentId = link.toNode;
    }

    return parentId;
}

} // namespace

SdfGraphNodeId GraphSystem::highlightNodeForSelection(const SdfGraph& graph)
{
    SdfGraphNodeId currentId = graph.selectedNode();
    if (currentId == 0 || currentId == graph.outputNode() || !producesValidSdf(graph, currentId)) {
        return 0;
    }

    const SdfGraphNode* selected = graph.node(currentId);
    if (selected != nullptr && isHighlightTransform(selected->payload.type)) {
        return currentId;
    }

    std::vector<SdfGraphNodeId> visited;
    SdfGraphNodeId highlightId = currentId;
    while (currentId != 0 && std::find(visited.begin(), visited.end(), currentId) == visited.end()) {
        visited.push_back(currentId);
        const SdfGraphNodeId parentId = singlePassThroughParent(graph, currentId);
        if (parentId == 0 || !producesValidSdf(graph, parentId)) {
            break;
        }
        highlightId = parentId;
        currentId = parentId;
    }

    return highlightId;
}

} // namespace sdf3d
