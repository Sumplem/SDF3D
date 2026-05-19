#include "sdf3d/systems/GraphSystem.h"

#include "sdf3d/scene/SdfNodeTraits.h"

#include <algorithm>
#include <unordered_set>

namespace sdf3d {
namespace {

bool hasValidSocket(const std::vector<std::string>& sockets, const std::string& socket)
{
    return std::find(sockets.begin(), sockets.end(), socket) != sockets.end();
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

bool producesValidSdfRecursive(const SdfGraph& graph, SdfGraphNodeId id, std::unordered_set<SdfGraphNodeId>& visiting)
{
    if (visiting.find(id) != visiting.end()) {
        return false;
    }

    const SdfGraphNode* node = graph.node(id);
    if (node == nullptr) {
        return false;
    }
    if (isSdfPrimitiveNode(node->payload.type)) {
        return true;
    }

    visiting.insert(id);
    std::vector<std::string> validSockets;
    std::size_t childCount = 0;
    for (const SdfGraphSocket& input : node->inputs) {
        if (input.type != SdfSocketType::Sdf) {
            continue;
        }
        const std::optional<SdfGraphLink> link = linkToInput(graph, id, input.name);
        if (link && producesValidSdfRecursive(graph, link->fromNode, visiting)) {
            validSockets.push_back(input.name);
            ++childCount;
        }
    }

    visiting.erase(id);
    return GraphSystem::loweredNodeHasRequiredInputs(node->payload.type, validSockets, childCount);
}

} // namespace

bool GraphSystem::loweredNodeHasRequiredInputs(SdfNodeType type, const std::vector<std::string>& validSockets, std::size_t childCount)
{
    if (isSdfPrimitiveNode(type)) {
        return true;
    }

    switch (type) {
    case SdfNodeType::Translate:
    case SdfNodeType::Rotate:
    case SdfNodeType::Scale:
    case SdfNodeType::Repeat:
    case SdfNodeType::Mirror:
    case SdfNodeType::Twist:
    case SdfNodeType::Bend:
        return hasValidSocket(validSockets, "child");
    case SdfNodeType::MaterialOverride:
        return hasValidSocket(validSockets, "sdf");
    case SdfNodeType::Subtract:
    case SdfNodeType::SmoothSubtract:
        return hasValidSocket(validSockets, "base");
    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion:
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect:
        return childCount > 0;
    case SdfNodeType::Output:
        return hasValidSocket(validSockets, "surface");
    default:
        return true;
    }
}

bool GraphSystem::producesValidSdf(const SdfGraph& graph, SdfGraphNodeId id)
{
    std::unordered_set<SdfGraphNodeId> visiting;
    return producesValidSdfRecursive(graph, id, visiting);
}

std::optional<SdfGraphLink> GraphSystem::effectiveLinkToInput(const SdfGraph& graph, SdfGraphNodeId id, const std::string& socket)
{
    const std::optional<SdfGraphLink> link = linkToInput(graph, id, socket);
    if (!link) {
        return std::nullopt;
    }

    return producesValidSdf(graph, link->fromNode) ? link : std::nullopt;
}

bool GraphSystem::nodeHasMissingRequiredInput(const SdfGraph& graph, const SdfGraphNode& node)
{
    if (isSdfPrimitiveNode(node.payload.type)) {
        return false;
    }

    for (const SdfGraphSocket& input : node.inputs) {
        if (input.type != SdfSocketType::Sdf) {
            continue;
        }
        if (!effectiveLinkToInput(graph, node.id, input.name)) {
            return true;
        }
    }

    return false;
}

} // namespace sdf3d
