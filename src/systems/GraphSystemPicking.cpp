#include "sdf3d/systems/GraphSystem.h"

#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/scene/SdfRotationParams.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_set>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace sdf3d {
namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float NO_HIT_DISTANCE = 1.0e6f;
constexpr float PICK_SURFACE_EPSILON = 0.0025f;
constexpr float PICK_MAX_DISTANCE = 100.0f;
constexpr int PICK_MAX_STEPS = 192;
constexpr float WARP_CORRECTION = 1.5f;

struct PickSample {
    float distance = NO_HIT_DISTANCE;
    SdfGraphNodeId node = 0;
    SdfGraphNodeId branchNode = 0;
};

float parameterOr(const SdfNode& node, const std::string& key, float fallback)
{
    const auto it = node.parameters.find(key);
    return it == node.parameters.end() ? fallback : it->second;
}

float radians(float degrees)
{
    return degrees * PI / 180.0f;
}

int socketOrder(const std::string& socket)
{
    if (socket == "child" || socket == "left" || socket == "base" || socket == "sdf") {
        return 0;
    }
    if (socket == "right" || socket == "cutter") {
        return 1;
    }
    return 100;
}

float sdBox(glm::vec3 point, glm::vec3 halfExtents)
{
    const glm::vec3 q = glm::abs(point) - halfExtents;
    return glm::length(glm::max(q, glm::vec3{0.0f})) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
}

float sdCylinder(glm::vec3 point, float radius, float halfHeight)
{
    const glm::vec2 d = glm::abs(glm::vec2{glm::length(glm::vec2{point.x, point.z}), point.y}) - glm::vec2{radius, halfHeight};
    return std::min(std::max(d.x, d.y), 0.0f) + glm::length(glm::max(d, glm::vec2{0.0f}));
}

float smoothMin(float a, float b, float k)
{
    const float h = std::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
    return glm::mix(b, a, h) - k * h * (1.0f - h);
}

glm::vec3 repeatPoint(glm::vec3 point, const SdfNode& node)
{
    const glm::vec3 cell = {
        std::max(parameterOr(node, "x", 2.0f), 0.0001f),
        std::max(parameterOr(node, "y", 2.0f), 0.0001f),
        std::max(parameterOr(node, "z", 2.0f), 0.0001f),
    };
    const bool repeatX = parameterOr(node, "repeatX", 1.0f) >= 0.5f;
    const bool repeatY = parameterOr(node, "repeatY", 1.0f) >= 0.5f;
    const bool repeatZ = parameterOr(node, "repeatZ", 1.0f) >= 0.5f;
    auto wrap = [](float value, float size) {
        float result = std::fmod(value + 0.5f * size, size);
        if (result < 0.0f) {
            result += size;
        }
        return result - 0.5f * size;
    };
    return {
        repeatX ? wrap(point.x, cell.x) : point.x,
        repeatY ? wrap(point.y, cell.y) : point.y,
        repeatZ ? wrap(point.z, cell.z) : point.z,
    };
}

glm::vec3 rotateAroundAxis(glm::vec3 point, int axis, float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    if (axis == 0) {
        return {point.x, c * point.y - s * point.z, s * point.y + c * point.z};
    }
    if (axis == 1) {
        return {c * point.x - s * point.z, point.y, s * point.x + c * point.z};
    }
    return {c * point.x - s * point.y, s * point.x + c * point.y, point.z};
}

std::vector<SdfGraphLink> effectiveInputs(const SdfGraph& graph, const SdfGraphNode& node)
{
    std::vector<SdfGraphLink> inputs;
    for (const SdfGraphLink& link : graph.links()) {
        if (link.toNode == node.id && GraphSystem::producesValidSdf(graph, link.fromNode)) {
            inputs.push_back(link);
        }
    }
    std::sort(inputs.begin(), inputs.end(), [](const SdfGraphLink& a, const SdfGraphLink& b) {
        const int orderA = socketOrder(a.toSocket);
        const int orderB = socketOrder(b.toSocket);
        if (orderA != orderB) {
            return orderA < orderB;
        }
        if (a.toSocket != b.toSocket) {
            return a.toSocket < b.toSocket;
        }
        return a.fromNode < b.fromNode;
    });
    return inputs;
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

SdfGraphNodeId nearestBranchTransform(const SdfGraph& graph, SdfGraphNodeId id)
{
    std::vector<SdfGraphNodeId> visited;
    SdfGraphNodeId currentId = id;
    while (currentId != 0 && std::find(visited.begin(), visited.end(), currentId) == visited.end()) {
        visited.push_back(currentId);
        const SdfGraphNode* current = graph.node(currentId);
        if (current == nullptr) {
            break;
        }
        if (isSdfTransformNode(current->payload.type)) {
            return currentId;
        }

        const std::optional<SdfGraphLink> child = linkToInput(graph, currentId, "child");
        if (child) {
            currentId = child->fromNode;
            continue;
        }
        const std::optional<SdfGraphLink> sdf = linkToInput(graph, currentId, "sdf");
        if (sdf) {
            currentId = sdf->fromNode;
            continue;
        }
        break;
    }

    return id;
}

PickSample evalNode(const SdfGraph& graph, SdfGraphNodeId id, glm::vec3 point, std::unordered_set<SdfGraphNodeId>& visiting);

PickSample evalFirstInput(const SdfGraph& graph, const SdfGraphNode& node, glm::vec3 point, std::unordered_set<SdfGraphNodeId>& visiting)
{
    const std::vector<SdfGraphLink> inputs = effectiveInputs(graph, node);
    if (inputs.empty()) {
        return {};
    }
    return evalNode(graph, inputs.front().fromNode, point, visiting);
}

PickSample evalBooleanInput(const SdfGraph& graph, const SdfGraphLink& input, glm::vec3 point, std::unordered_set<SdfGraphNodeId>& visiting)
{
    PickSample sample = evalNode(graph, input.fromNode, point, visiting);
    if (sample.node != 0) {
        sample.branchNode = nearestBranchTransform(graph, input.fromNode);
    }
    return sample;
}

PickSample evalNode(const SdfGraph& graph, SdfGraphNodeId id, glm::vec3 point, std::unordered_set<SdfGraphNodeId>& visiting)
{
    if (visiting.find(id) != visiting.end()) {
        return {};
    }

    const SdfGraphNode* graphNode = graph.node(id);
    if (graphNode == nullptr) {
        return {};
    }
    const SdfNode& node = graphNode->payload;

    visiting.insert(id);
    PickSample result;
    switch (node.type) {
    case SdfNodeType::Sphere:
        result = {glm::length(point) - parameterOr(node, "radius", 1.0f), id};
        break;
    case SdfNodeType::Box:
        result = {sdBox(point, {parameterOr(node, "x", 1.0f), parameterOr(node, "y", 1.0f), parameterOr(node, "z", 1.0f)}), id};
        break;
    case SdfNodeType::Cylinder:
        result = {sdCylinder(point, parameterOr(node, "radius", 1.0f), parameterOr(node, "halfHeight", 1.0f)), id};
        break;
    case SdfNodeType::Torus: {
        const float majorRadius = parameterOr(node, "majorRadius", 1.0f);
        const float minorRadius = parameterOr(node, "minorRadius", 0.25f);
        result = {glm::length(glm::vec2{glm::length(glm::vec2{point.x, point.z}) - majorRadius, point.y}) - minorRadius, id};
        break;
    }
    case SdfNodeType::Plane: {
        const glm::vec3 normal = glm::normalize(glm::vec3{
            parameterOr(node, "normalX", 0.0f),
            parameterOr(node, "normalY", 1.0f),
            parameterOr(node, "normalZ", 0.0f),
        });
        result = {glm::dot(point, normal) + parameterOr(node, "offset", 0.0f), id};
        break;
    }
    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion:
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect: {
        const std::vector<SdfGraphLink> inputs = effectiveInputs(graph, *graphNode);
        if (inputs.empty()) {
            break;
        }
        result = evalBooleanInput(graph, inputs.front(), point, visiting);
        for (std::size_t i = 1; i < inputs.size(); ++i) {
            const PickSample child = evalBooleanInput(graph, inputs[i], point, visiting);
            const bool useChild = node.type == SdfNodeType::Union || node.type == SdfNodeType::SmoothUnion
                ? child.distance < result.distance
                : child.distance > result.distance;
            if (node.type == SdfNodeType::SmoothUnion) {
                result.distance = smoothMin(result.distance, child.distance, std::max(parameterOr(node, "smoothness", 0.25f), 0.0001f));
                if (useChild) {
                    result.node = child.node;
                }
            } else if (node.type == SdfNodeType::SmoothIntersect) {
                result.distance = -smoothMin(-result.distance, -child.distance, std::max(parameterOr(node, "smoothness", 0.25f), 0.0001f));
                if (useChild) {
                    result.node = child.node;
                }
            } else if (useChild) {
                result = child;
            }
        }
        break;
    }
    case SdfNodeType::Subtract:
    case SdfNodeType::SmoothSubtract: {
        const std::vector<SdfGraphLink> inputs = effectiveInputs(graph, *graphNode);
        if (inputs.empty()) {
            break;
        }
        PickSample base = evalBooleanInput(graph, inputs.front(), point, visiting);
        if (inputs.size() == 1) {
            result = base;
            break;
        }
        const PickSample cutter = evalBooleanInput(graph, inputs[1], point, visiting);
        result = node.type == SdfNodeType::Subtract
            ? PickSample{std::max(-cutter.distance, base.distance), base.node}
            : PickSample{-smoothMin(-base.distance, cutter.distance, std::max(parameterOr(node, "smoothness", 0.25f), 0.0001f)), base.node};
        break;
    }
    case SdfNodeType::Translate:
        result = evalFirstInput(graph, *graphNode, point - glm::vec3{parameterOr(node, "x", 0.0f), parameterOr(node, "y", 0.0f), parameterOr(node, "z", 0.0f)}, visiting);
        break;
    case SdfNodeType::Rotate: {
        result = evalFirstInput(graph, *graphNode, glm::transpose(rotationMatrixFromQuaternion(rotationQuaternionForNode(node))) * point, visiting);
        break;
    }
    case SdfNodeType::Scale: {
        const float uniformScale = parameterOr(node, "scale", 1.0f);
        const glm::vec3 scale = {
            std::max(parameterOr(node, "x", uniformScale), 0.0001f),
            std::max(parameterOr(node, "y", uniformScale), 0.0001f),
            std::max(parameterOr(node, "z", uniformScale), 0.0001f),
        };
        result = evalFirstInput(graph, *graphNode, point / scale, visiting);
        result.distance *= std::min({scale.x, scale.y, scale.z});
        break;
    }
    case SdfNodeType::Repeat:
        result = evalFirstInput(graph, *graphNode, repeatPoint(point, node), visiting);
        break;
    case SdfNodeType::Mirror:
        result = evalFirstInput(graph, *graphNode, {
            parameterOr(node, "x", 1.0f) >= 0.5f ? std::abs(point.x) : point.x,
            parameterOr(node, "y", 0.0f) >= 0.5f ? std::abs(point.y) : point.y,
            parameterOr(node, "z", 0.0f) >= 0.5f ? std::abs(point.z) : point.z,
        }, visiting);
        break;
    case SdfNodeType::Twist:
    case SdfNodeType::Bend: {
        const float strength = parameterOr(node, "strength", node.type == SdfNodeType::Twist ? 1.0f : 0.5f);
        const int axis = static_cast<int>(std::clamp(parameterOr(node, "axis", node.type == SdfNodeType::Twist ? 1.0f : 0.0f), 0.0f, 2.0f) + 0.5f);
        const float axisCoord = axis == 0 ? point.x : (axis == 1 ? point.y : point.z);
        result = evalFirstInput(graph, *graphNode, rotateAroundAxis(point, axis, axisCoord * strength), visiting);
        result.distance /= (1.0f + std::abs(strength) * WARP_CORRECTION);
        break;
    }
    case SdfNodeType::MaterialOverride:
    case SdfNodeType::Output:
        result = evalFirstInput(graph, *graphNode, point, visiting);
        break;
    default:
        break;
    }

    visiting.erase(id);
    return result;
}

std::optional<SdfGraphLink> outputSurfaceLink(const SdfGraph& graph)
{
    return GraphSystem::effectiveLinkToInput(graph, graph.outputNode(), "surface");
}

std::optional<SdfGraphNodeId> pickBooleanInputByRay(
    const SdfGraph& graph,
    const SdfGraphNode& node,
    glm::vec3 rayOrigin,
    glm::vec3 direction);

std::optional<SdfGraphNodeId> pickBranchByRay(
    const SdfGraph& graph,
    const SdfGraphLink& branch,
    glm::vec3 rayOrigin,
    glm::vec3 direction)
{
    const SdfGraphNode* branchNode = graph.node(branch.fromNode);
    if (branchNode != nullptr && isSdfBooleanNode(branchNode->payload.type)) {
        return pickBooleanInputByRay(graph, *branchNode, rayOrigin, direction);
    }

    float traveled = 0.0f;
    for (int step = 0; step < PICK_MAX_STEPS && traveled < PICK_MAX_DISTANCE; ++step) {
        std::unordered_set<SdfGraphNodeId> visiting;
        const PickSample sample = evalNode(graph, branch.fromNode, rayOrigin + direction * traveled, visiting);
        if (sample.node != 0 && sample.distance <= PICK_SURFACE_EPSILON) {
            return nearestBranchTransform(graph, branch.fromNode);
        }
        traveled += std::max(sample.distance, PICK_SURFACE_EPSILON);
    }

    return std::nullopt;
}

std::optional<SdfGraphNodeId> pickBooleanInputByRay(
    const SdfGraph& graph,
    const SdfGraphNode& node,
    glm::vec3 rayOrigin,
    glm::vec3 direction)
{
    const std::vector<SdfGraphLink> inputs = effectiveInputs(graph, node);
    for (const SdfGraphLink& input : inputs) {
        const std::optional<SdfGraphNodeId> picked = pickBranchByRay(graph, input, rayOrigin, direction);
        if (picked) {
            return picked;
        }
    }

    return std::nullopt;
}

} // namespace

SdfGraphNodeId GraphSystem::pickNodeByRay(const SdfGraph& graph, glm::vec3 rayOrigin, glm::vec3 rayDirection)
{
    const std::optional<SdfGraphLink> root = outputSurfaceLink(graph);
    if (!root || glm::length(rayDirection) <= 0.0001f) {
        return 0;
    }

    const glm::vec3 direction = glm::normalize(rayDirection);
    const SdfGraphNode* rootNode = graph.node(root->fromNode);
    if (rootNode != nullptr && isSdfBooleanNode(rootNode->payload.type)) {
        if (const std::optional<SdfGraphNodeId> pickedBranch = pickBooleanInputByRay(graph, *rootNode, rayOrigin, direction)) {
            return *pickedBranch;
        }
    }

    float traveled = 0.0f;
    for (int step = 0; step < PICK_MAX_STEPS && traveled < PICK_MAX_DISTANCE; ++step) {
        std::unordered_set<SdfGraphNodeId> visiting;
        const PickSample sample = evalNode(graph, root->fromNode, rayOrigin + direction * traveled, visiting);
        if (sample.node != 0 && sample.distance <= PICK_SURFACE_EPSILON) {
            return sample.branchNode != 0 ? sample.branchNode : sample.node;
        }
        traveled += std::max(sample.distance, PICK_SURFACE_EPSILON);
    }

    return 0;
}

} // namespace sdf3d
