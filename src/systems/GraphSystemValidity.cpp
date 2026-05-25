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

bool isBypassInput(SdfNodeType type, const std::string& socket)
{
    switch (type) {
    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion:
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect:
        return socket == "inputs";
    case SdfNodeType::Subtract:
    case SdfNodeType::SmoothSubtract:
        return socket == "base";
    default:
        return false;
    }
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

std::vector<SdfGraphLink> linksToInput(const SdfGraph& graph, SdfGraphNodeId node, const std::string& socket)
{
    std::vector<SdfGraphLink> links;
    for (const SdfGraphLink& link : graph.links()) {
        if (link.toNode == node && link.toSocket == socket) {
            links.push_back(link);
        }
    }

    return links;
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
        for (const SdfGraphLink& link : linksToInput(graph, id, input.name)) {
            if (producesValidSdfRecursive(graph, link.fromNode, visiting)) {
                validSockets.push_back(input.name);
                ++childCount;
            }
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

std::vector<SdfGraphLink> GraphSystem::effectiveLinksToInput(const SdfGraph& graph, SdfGraphNodeId id, const std::string& socket)
{
    std::vector<SdfGraphLink> links;
    for (const SdfGraphLink& link : linksToInput(graph, id, socket)) {
        if (producesValidSdf(graph, link.fromNode)) {
            links.push_back(link);
        }
    }

    return links;
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
        const std::vector<SdfGraphLink> declaredLinks = linksToInput(graph, node.id, input.name);
        const std::vector<SdfGraphLink> effectiveLinks = effectiveLinksToInput(graph, node.id, input.name);
        if (effectiveLinks.empty() || effectiveLinks.size() != declaredLinks.size()) {
            return true;
        }
    }

    return false;
}

std::optional<SdfGraphLink> GraphSystem::effectiveBypassSourceLink(const SdfGraph& graph, const SdfGraphNode& node)
{
    if (!nodeHasMissingRequiredInput(graph, node)) {
        return std::nullopt;
    }

    for (const SdfGraphSocket& input : node.inputs) {
        if (input.type != SdfSocketType::Sdf || !isBypassInput(node.payload.type, input.name)) {
            continue;
        }

        std::vector<SdfGraphLink> links = effectiveLinksToInput(graph, node.id, input.name);
        if (!links.empty()) {
            return links.front();
        }
    }

    return std::nullopt;
}

} // namespace sdf3d
