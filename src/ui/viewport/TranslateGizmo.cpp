#include "sdf3d/ui/viewport/TranslateGizmo.h"

#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/systems/GraphSystem.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace sdf3d {
namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float AXIS_LENGTH = 1.0f;
constexpr float AXIS_RADIUS = 0.035f;
constexpr float RING_RADIUS = 0.85f;
constexpr float RING_TUBE_RADIUS = 0.025f;
constexpr float GIZMO_HIT_EPSILON = 0.01f;
constexpr float GIZMO_MAX_DISTANCE = 100.0f;
constexpr int GIZMO_TYPE_TRANSLATE = 0;
constexpr int GIZMO_TYPE_ROTATE = 1;
constexpr int GIZMO_TYPE_SCALE = 2;
constexpr float MIN_SCALE = 0.001f;

float radians(float degrees)
{
    return degrees * PI / 180.0f;
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

glm::vec3 scaleValues(const SdfGraphNode& node)
{
    const float uniformScale = parameterOr(node.payload, "scale", 1.0f);
    return {
        parameterOr(node.payload, "x", uniformScale),
        parameterOr(node.payload, "y", uniformScale),
        parameterOr(node.payload, "z", uniformScale),
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

SdfGraphNodeId findPassThroughChildOfType(const SdfGraph& graph, SdfGraphNodeId id, SdfNodeType type)
{
    std::vector<SdfGraphNodeId> visited;
    SdfGraphNodeId currentId = id;
    while (currentId != 0 && std::find(visited.begin(), visited.end(), currentId) == visited.end()) {
        visited.push_back(currentId);
        const std::optional<SdfGraphLink> child = linkToInput(graph, currentId, "child");
        const std::optional<SdfGraphLink> sdf = child ? child : linkToInput(graph, currentId, "sdf");
        if (!sdf) {
            return 0;
        }

        const SdfGraphNode* upstream = graph.node(sdf->fromNode);
        if (upstream == nullptr || (!isPrimitiveNode(upstream->payload.type) && !isTransformPassThroughNode(upstream->payload.type))) {
            return 0;
        }
        if (upstream->payload.type == type) {
            return upstream->id;
        }
        if (!isTransformPassThroughNode(upstream->payload.type)) {
            return 0;
        }
        currentId = upstream->id;
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

SdfGraphNodeId findUpstreamTranslate(const SdfGraph& graph, SdfGraphNodeId id)
{
    std::vector<SdfGraphNodeId> visited;
    SdfGraphNodeId currentId = id;
    while (currentId != 0 && std::find(visited.begin(), visited.end(), currentId) == visited.end()) {
        visited.push_back(currentId);
        const SdfGraphNode* current = graph.node(currentId);
        if (current == nullptr) {
            break;
        }
        if (current->payload.type == SdfNodeType::Translate) {
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
    return 0;
}

glm::vec3 gizmoOriginForNode(const SdfGraph& graph, SdfGraphNodeId id)
{
    if (const SdfGraphNodeId upstreamTranslate = findUpstreamTranslate(graph, id)) {
        return GraphSystem::accumulatedTranslatePosition(graph, upstreamTranslate);
    }
    if (const SdfGraphNodeId downstreamTranslate = findPassThroughParentOfType(graph, id, SdfNodeType::Translate)) {
        return GraphSystem::accumulatedTranslatePosition(graph, downstreamTranslate);
    }
    return {0.0f, 0.0f, 0.0f};
}

glm::mat3 gizmoOrientationForNode(const SdfGraph& graph, SdfGraphNodeId id)
{
    const SdfGraphNodeId rotateId = findPassThroughNodeOfTypeInChain(graph, id, SdfNodeType::Rotate);
    const SdfGraphNode* rotate = graph.node(rotateId);
    return rotate == nullptr ? glm::mat3{1.0f} : rotationMatrixFromQuaternion(rotationQuaternionForNode(rotate->payload));
}

std::optional<glm::vec3> screenRayDirection(ImVec2 screen, const RenderCamera& camera, ImVec2 imageMin, ImVec2 imageMax)
{
    const float width = std::max(1.0f, imageMax.x - imageMin.x);
    const float height = std::max(1.0f, imageMax.y - imageMin.y);
    const float ndcX = ((screen.x - imageMin.x) / width) * 2.0f - 1.0f;
    const float ndcY = 1.0f - ((screen.y - imageMin.y) / height) * 2.0f;

    const glm::mat4 view = glm::lookAt(camera.position, camera.target, camera.up);
    const glm::mat4 projection = glm::perspective(radians(camera.fovDegrees), width / height, 0.01f, 100.0f);
    const glm::mat4 inverseViewProjection = glm::inverse(projection * view);
    const glm::vec4 nearWorld = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    const glm::vec4 farWorld = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearWorld.w) <= 0.0001f || std::abs(farWorld.w) <= 0.0001f) {
        return std::nullopt;
    }

    const glm::vec3 nearPoint = glm::vec3(nearWorld) / nearWorld.w;
    const glm::vec3 farPoint = glm::vec3(farWorld) / farWorld.w;
    const glm::vec3 direction = farPoint - nearPoint;
    if (glm::length(direction) <= 0.0001f) {
        return std::nullopt;
    }

    return glm::normalize(direction);
}

std::optional<float> axisParameterFromMouse(
    ImVec2 mouse,
    glm::vec3 axisOrigin,
    glm::vec3 axisDirection,
    const RenderCamera& camera,
    ImVec2 imageMin,
    ImVec2 imageMax)
{
    const std::optional<glm::vec3> rayDirection = screenRayDirection(mouse, camera, imageMin, imageMax);
    if (!rayDirection) {
        return std::nullopt;
    }

    const glm::vec3 offset = camera.position - axisOrigin;
    const float axisRayDot = glm::dot(axisDirection, *rayDirection);
    const float denominator = 1.0f - axisRayDot * axisRayDot;
    if (std::abs(denominator) <= 0.0001f) {
        return std::nullopt;
    }

    const float axisOffsetDot = glm::dot(axisDirection, offset);
    const float rayOffsetDot = glm::dot(*rayDirection, offset);
    return (axisOffsetDot - axisRayDot * rayOffsetDot) / denominator;
}

float sdCapsule(glm::vec3 point, glm::vec3 a, glm::vec3 b, float radius)
{
    const glm::vec3 pa = point - a;
    const glm::vec3 ba = b - a;
    const float h = std::clamp(glm::dot(pa, ba) / std::max(glm::dot(ba, ba), 0.0001f), 0.0f, 1.0f);
    return glm::length(pa - ba * h) - radius;
}

float sdTorus(glm::vec3 point, float radius, float tubeRadius)
{
    const glm::vec2 q = {glm::length(glm::vec2{point.x, point.z}) - radius, point.y};
    return glm::length(q) - tubeRadius;
}

float sdBox(glm::vec3 point, glm::vec3 halfSize)
{
    const glm::vec3 q = glm::abs(point) - halfSize;
    return glm::length(glm::max(q, glm::vec3{0.0f})) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
}

float axisDistance(glm::vec3 point, glm::vec3 origin, glm::vec3 axis)
{
    return sdCapsule(point, origin, origin + axis * AXIS_LENGTH, AXIS_RADIUS);
}

float rotateAxisDistance(glm::vec3 point, glm::vec3 origin, int axis, glm::mat3 orientation)
{
    const glm::vec3 local = glm::transpose(orientation) * (point - origin);
    if (axis == 0) {
        return sdTorus({local.z, local.x, local.y}, RING_RADIUS, RING_TUBE_RADIUS);
    }
    if (axis == 1) {
        return sdTorus(local, RING_RADIUS, RING_TUBE_RADIUS);
    }
    return sdTorus({local.x, local.z, local.y}, RING_RADIUS, RING_TUBE_RADIUS);
}

float scaleAxisDistance(glm::vec3 point, glm::vec3 origin, glm::vec3 axis, glm::mat3 orientation)
{
    const glm::vec3 halfSize = glm::vec3{AXIS_RADIUS * 2.5f};
    return sdBox(glm::transpose(orientation) * (point - (origin + axis * AXIS_LENGTH)), halfSize);
}

std::optional<int> hitTranslateAxis(ImVec2 mouse, glm::vec3 origin, const RenderCamera& camera, ImVec2 imageMin, ImVec2 imageMax)
{
    const std::optional<glm::vec3> rayDirection = screenRayDirection(mouse, camera, imageMin, imageMax);
    if (!rayDirection) {
        return std::nullopt;
    }

    const glm::vec3 axes[] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };

    float bestHitDistance = GIZMO_MAX_DISTANCE;
    int bestAxis = -1;
    for (int axis = 0; axis < 3; ++axis) {
        float distanceTraveled = 0.0f;
        for (int step = 0; step < 128 && distanceTraveled < GIZMO_MAX_DISTANCE; ++step) {
            const glm::vec3 samplePoint = camera.position + *rayDirection * distanceTraveled;
            const float distanceToAxis = axisDistance(samplePoint, origin, axes[axis]);
            if (distanceToAxis <= GIZMO_HIT_EPSILON) {
                if (distanceTraveled < bestHitDistance) {
                    bestHitDistance = distanceTraveled;
                    bestAxis = axis;
                }
                break;
            }
            distanceTraveled += std::max(distanceToAxis, GIZMO_HIT_EPSILON);
        }
    }

    if (bestAxis >= 0) {
        return bestAxis;
    }
    return std::nullopt;
}

std::optional<int> hitRotateAxis(ImVec2 mouse, glm::vec3 origin, glm::mat3 orientation, const RenderCamera& camera, ImVec2 imageMin, ImVec2 imageMax)
{
    const std::optional<glm::vec3> rayDirection = screenRayDirection(mouse, camera, imageMin, imageMax);
    if (!rayDirection) {
        return std::nullopt;
    }

    float bestHitDistance = GIZMO_MAX_DISTANCE;
    int bestAxis = -1;
    for (int axis = 0; axis < 3; ++axis) {
        float distanceTraveled = 0.0f;
        for (int step = 0; step < 128 && distanceTraveled < GIZMO_MAX_DISTANCE; ++step) {
            const glm::vec3 samplePoint = camera.position + *rayDirection * distanceTraveled;
            const float distanceToAxis = rotateAxisDistance(samplePoint, origin, axis, orientation);
            if (distanceToAxis <= GIZMO_HIT_EPSILON) {
                if (distanceTraveled < bestHitDistance) {
                    bestHitDistance = distanceTraveled;
                    bestAxis = axis;
                }
                break;
            }
            distanceTraveled += std::max(distanceToAxis, GIZMO_HIT_EPSILON);
        }
    }

    if (bestAxis >= 0) {
        return bestAxis;
    }
    return std::nullopt;
}

std::optional<int> hitScaleAxis(ImVec2 mouse, glm::vec3 origin, glm::mat3 orientation, const RenderCamera& camera, ImVec2 imageMin, ImVec2 imageMax)
{
    const std::optional<glm::vec3> rayDirection = screenRayDirection(mouse, camera, imageMin, imageMax);
    if (!rayDirection) {
        return std::nullopt;
    }

    const glm::vec3 axes[] = {
        orientation * glm::vec3{1.0f, 0.0f, 0.0f},
        orientation * glm::vec3{0.0f, 1.0f, 0.0f},
        orientation * glm::vec3{0.0f, 0.0f, 1.0f},
    };

    float bestHitDistance = GIZMO_MAX_DISTANCE;
    int bestAxis = -1;
    for (int axis = 0; axis < 3; ++axis) {
        float distanceTraveled = 0.0f;
        for (int step = 0; step < 128 && distanceTraveled < GIZMO_MAX_DISTANCE; ++step) {
            const glm::vec3 samplePoint = camera.position + *rayDirection * distanceTraveled;
            const float distanceToAxis = scaleAxisDistance(samplePoint, origin, axes[axis], orientation);
            if (distanceToAxis <= GIZMO_HIT_EPSILON) {
                if (distanceTraveled < bestHitDistance) {
                    bestHitDistance = distanceTraveled;
                    bestAxis = axis;
                }
                break;
            }
            distanceTraveled += std::max(distanceToAxis, GIZMO_HIT_EPSILON);
        }
    }

    if (bestAxis >= 0) {
        return bestAxis;
    }
    return std::nullopt;
}

std::optional<float> angleFromMouse(
    ImVec2 mouse,
    glm::vec3 origin,
    int axis,
    glm::mat3 orientation,
    const RenderCamera& camera,
    ImVec2 imageMin,
    ImVec2 imageMax)
{
    const std::optional<glm::vec3> rayDirection = screenRayDirection(mouse, camera, imageMin, imageMax);
    if (!rayDirection) {
        return std::nullopt;
    }

    const glm::vec3 normals[] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };
    const glm::vec3 normal = orientation * normals[axis];
    const float denominator = glm::dot(*rayDirection, normal);
    if (std::abs(denominator) <= 0.0001f) {
        return std::nullopt;
    }

    const float t = glm::dot(origin - camera.position, normal) / denominator;
    if (t <= 0.0f) {
        return std::nullopt;
    }

    const glm::vec3 local = glm::transpose(orientation) * (camera.position + *rayDirection * t - origin);
    if (glm::length(local) <= 0.0001f) {
        return std::nullopt;
    }

    if (axis == 0) {
        return std::atan2(local.z, local.y);
    }
    if (axis == 1) {
        return std::atan2(local.x, local.z);
    }
    return std::atan2(local.y, local.x);
}

} // namespace

bool TranslateGizmo::active() const
{
    return m_activeAxis >= 0;
}

void TranslateGizmo::drawSettings()
{
    if (ImGui::IsKeyPressed(ImGuiKey_W)) {
        m_mode = Mode::Translate;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E)) {
        m_mode = Mode::Rotate;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R)) {
        m_mode = Mode::Scale;
    }

    ImGui::SetNextItemWidth(96.0f);
    int mode = m_mode == Mode::Translate ? 0 : (m_mode == Mode::Rotate ? 1 : 2);
    const char* modeLabels[] = {"Move", "Rotate", "Scale"};
    if (ImGui::Combo("##gizmo_mode", &mode, modeLabels, 3)) {
        m_mode = mode == 0 ? Mode::Translate : (mode == 1 ? Mode::Rotate : Mode::Scale);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(96.0f);
    int style = m_rotateStyle == RotateStyle::Rings ? 0 : 1;
    const char* styleLabels[] = {"Rings", "Axis Arcs"};
    if (m_mode != Mode::Rotate) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Combo("##rotate_style", &style, styleLabels, 2)) {
        m_rotateStyle = style == 0 ? RotateStyle::Rings : RotateStyle::AxisArcs;
    }
    if (m_mode != Mode::Rotate) {
        ImGui::EndDisabled();
    }
    if (m_rotateStyle == RotateStyle::AxisArcs) {
        m_rotateStyle = RotateStyle::Rings;
    }
}

EditorDirtyState TranslateGizmo::update(SceneGraph& sceneGraph, const RenderCamera& camera, ImVec2 imageMin, ImVec2 imageMax, RenderGizmo& gizmo)
{
    EditorDirtyState dirty;
    gizmo = {};
    SdfGraph& graph = sceneGraph.graph();
    SdfGraphNodeId selectedId = graph.selectedNode();
    SdfGraphNode* selected = graph.node(selectedId);
    SdfNodeType activeType = SdfNodeType::Translate;
    if (m_mode == Mode::Rotate) {
        activeType = SdfNodeType::Rotate;
    } else if (m_mode == Mode::Scale) {
        activeType = SdfNodeType::Scale;
    }
    if (selected == nullptr || (selected->payload.type != activeType && !isPrimitiveNode(selected->payload.type) && !isTransformPassThroughNode(selected->payload.type))) {
        m_activeAxis = -1;
        return dirty;
    }

    SdfGraphNodeId gizmoNodeId = selectedId;
    SdfGraphNode* gizmoNode = selected;
    if (selected->payload.type != activeType) {
        SdfGraphNodeId existingId = findPassThroughNodeOfTypeInChain(graph, selectedId, activeType);
        if (existingId != 0) {
            if (SdfGraphNode* existing = graph.node(existingId)) {
                gizmoNodeId = existingId;
                gizmoNode = existing;
            }
        }
    }

    glm::vec3 origin = gizmoOriginForNode(graph, gizmoNodeId);
    glm::mat3 orientation = glm::mat3{1.0f};
    if (m_mode == Mode::Rotate || m_mode == Mode::Scale) {
        orientation = gizmoOrientationForNode(graph, gizmoNodeId);
    }
    const glm::vec3 worldAxes[] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    int hoveredAxis = -1;
    if (hovered) {
        if (m_mode == Mode::Translate) {
            hoveredAxis = hitTranslateAxis(mouse, origin, camera, imageMin, imageMax).value_or(-1);
        } else if (m_mode == Mode::Rotate) {
            hoveredAxis = hitRotateAxis(mouse, origin, orientation, camera, imageMin, imageMax).value_or(-1);
        } else {
            hoveredAxis = hitScaleAxis(mouse, origin, orientation, camera, imageMin, imageMax).value_or(-1);
        }
    }

    gizmo.visible = true;
    gizmo.center = origin;
    gizmo.orientation = orientation;
    gizmo.arrowLength = AXIS_LENGTH;
    gizmo.arrowRadius = AXIS_RADIUS;
    gizmo.ringRadius = RING_RADIUS;
    gizmo.tubeRadius = RING_TUBE_RADIUS;
    gizmo.activeAxis = m_activeAxis;
    gizmo.hoverAxis = hoveredAxis;
    gizmo.type = GIZMO_TYPE_TRANSLATE;
    if (m_mode == Mode::Rotate) {
        gizmo.type = GIZMO_TYPE_ROTATE;
    } else if (m_mode == Mode::Scale) {
        gizmo.type = GIZMO_TYPE_SCALE;
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredAxis >= 0) {
        if (gizmoNode->payload.type == activeType) {
            selectedId = gizmoNodeId;
            selected = gizmoNode;
            graph.setSelectedNode(selectedId);
        } else {
            if (m_mode == Mode::Translate) {
                selectedId = GraphSystem::ensureTranslateWrapperForNode(graph, selectedId);
            } else if (m_mode == Mode::Rotate) {
                selectedId = GraphSystem::ensureRotateWrapperForNode(graph, selectedId);
            } else {
                selectedId = GraphSystem::ensureScaleWrapperForNode(graph, selectedId);
            }
            selected = graph.node(selectedId);
            dirty.scene = true;
            if (selected == nullptr) {
                return dirty;
            }
        }
        if (m_mode == Mode::Translate) {
            origin = GraphSystem::accumulatedTranslatePosition(graph, selectedId);
        } else {
            origin = gizmoOriginForNode(graph, selectedId);
        }
        orientation = (m_mode == Mode::Rotate || m_mode == Mode::Scale) ? gizmoOrientationForNode(graph, selectedId) : glm::mat3{1.0f};
        gizmo.orientation = orientation;
        m_dragOrientation = orientation;
        const glm::vec3 dragAxes[] = {
            m_mode == Mode::Scale ? orientation * glm::vec3{1.0f, 0.0f, 0.0f} : worldAxes[0],
            m_mode == Mode::Scale ? orientation * glm::vec3{0.0f, 1.0f, 0.0f} : worldAxes[1],
            m_mode == Mode::Scale ? orientation * glm::vec3{0.0f, 0.0f, 1.0f} : worldAxes[2],
        };
        m_activeAxis = hoveredAxis;
        m_dragWorldOrigin = origin;
        m_dragStartLocalPosition = translatePosition(*selected);
        m_dragStartQuaternion = selected->payload.type == SdfNodeType::Rotate ? rotationQuaternionForNode(selected->payload) : glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
        m_dragStartScale = scaleValues(*selected)[m_activeAxis];
        m_dragStartAxisT = axisParameterFromMouse(mouse, m_dragWorldOrigin, dragAxes[m_activeAxis], camera, imageMin, imageMax).value_or(0.0f);
        m_dragStartAngle = angleFromMouse(mouse, m_dragWorldOrigin, m_activeAxis, m_dragOrientation, camera, imageMin, imageMax).value_or(0.0f);
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        m_activeAxis = -1;
    }

    glm::vec3 activeGizmoCenter = m_dragWorldOrigin;
    if (m_activeAxis >= 0 && selected != nullptr && selected->payload.type == SdfNodeType::Translate) {
        const std::optional<float> currentAxisT = axisParameterFromMouse(mouse, m_dragWorldOrigin, worldAxes[m_activeAxis], camera, imageMin, imageMax);
        if (currentAxisT) {
            const glm::vec3 delta = worldAxes[m_activeAxis] * (*currentAxisT - m_dragStartAxisT);
            const glm::vec3 moved = m_dragStartLocalPosition + delta;
            selected->payload.parameters["x"] = moved.x;
            selected->payload.parameters["y"] = moved.y;
            selected->payload.parameters["z"] = moved.z;
            activeGizmoCenter = m_dragWorldOrigin + delta;
            dirty.params = true;
        }
    }
    if (m_activeAxis >= 0 && selected != nullptr && selected->payload.type == SdfNodeType::Rotate) {
        const std::optional<float> currentAngle = angleFromMouse(mouse, m_dragWorldOrigin, m_activeAxis, m_dragOrientation, camera, imageMin, imageMax);
        if (currentAngle) {
            const glm::vec4 delta = rotationAxisAngleQuaternion(m_activeAxis, *currentAngle - m_dragStartAngle);
            const glm::vec4 moved = multiplyRotationQuaternion(m_dragStartQuaternion, delta);
            storeRotationQuaternion(selected->payload, moved);
            const glm::vec3 displayDegrees = rotationEulerDegreesFromQuaternion(moved);
            selected->payload.parameters["xDegrees"] = displayDegrees.x;
            selected->payload.parameters["yDegrees"] = displayDegrees.y;
            selected->payload.parameters["zDegrees"] = displayDegrees.z;
            dirty.params = true;
        }
    }
    if (m_activeAxis >= 0 && selected != nullptr && selected->payload.type == SdfNodeType::Scale) {
        const glm::vec3 dragScaleAxes[] = {
            m_dragOrientation * glm::vec3{1.0f, 0.0f, 0.0f},
            m_dragOrientation * glm::vec3{0.0f, 1.0f, 0.0f},
            m_dragOrientation * glm::vec3{0.0f, 0.0f, 1.0f},
        };
        const std::optional<float> currentAxisT = axisParameterFromMouse(mouse, m_dragWorldOrigin, dragScaleAxes[m_activeAxis], camera, imageMin, imageMax);
        if (currentAxisT) {
            const float moved = std::max(MIN_SCALE, m_dragStartScale + (*currentAxisT - m_dragStartAxisT));
            if (m_activeAxis == 0) {
                selected->payload.parameters["x"] = moved;
            } else if (m_activeAxis == 1) {
                selected->payload.parameters["y"] = moved;
            } else {
                selected->payload.parameters["z"] = moved;
            }
            dirty.params = true;
        }
    }

    gizmo.center = m_activeAxis >= 0 ? activeGizmoCenter : origin;
    gizmo.activeAxis = m_activeAxis;
    return dirty;
}

} // namespace sdf3d
