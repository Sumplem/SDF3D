#include "sdf3d/ui/viewport/TranslateGizmo.h"

#include "sdf3d/systems/GraphSystem.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace sdf3d {
namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float AXIS_LENGTH = 1.0f;
constexpr float HIT_DISTANCE = 10.0f;
constexpr float LINE_THICKNESS = 4.0f;
constexpr float ACTIVE_LINE_THICKNESS = 6.0f;
constexpr float HOVER_LINE_THICKNESS = 5.0f;

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

float distancePointToSegment(ImVec2 point, ImVec2 a, ImVec2 b)
{
    const ImVec2 ab = {b.x - a.x, b.y - a.y};
    const ImVec2 ap = {point.x - a.x, point.y - a.y};
    const float lengthSquared = ab.x * ab.x + ab.y * ab.y;
    if (lengthSquared <= 0.0001f) {
        const float dx = point.x - a.x;
        const float dy = point.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    const float t = std::clamp((ap.x * ab.x + ap.y * ab.y) / lengthSquared, 0.0f, 1.0f);
    const ImVec2 closest = {a.x + ab.x * t, a.y + ab.y * t};
    const float dx = point.x - closest.x;
    const float dy = point.y - closest.y;
    return std::sqrt(dx * dx + dy * dy);
}

std::optional<ImVec2> projectWorldToScreen(glm::vec3 world, const RenderCamera& camera, ImVec2 imageMin, ImVec2 imageMax)
{
    const float width = std::max(1.0f, imageMax.x - imageMin.x);
    const float height = std::max(1.0f, imageMax.y - imageMin.y);
    const glm::mat4 view = glm::lookAt(camera.position, camera.target, camera.up);
    const glm::mat4 projection = glm::perspective(radians(camera.fovDegrees), width / height, 0.01f, 100.0f);
    const glm::vec4 clip = projection * view * glm::vec4(world, 1.0f);
    if (clip.w <= 0.0001f) {
        return std::nullopt;
    }

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.z < -1.0f || ndc.z > 1.0f) {
        return std::nullopt;
    }

    return ImVec2{
        imageMin.x + (ndc.x * 0.5f + 0.5f) * width,
        imageMin.y + (0.5f - ndc.y * 0.5f) * height,
    };
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

} // namespace

bool TranslateGizmo::active() const
{
    return m_activeAxis >= 0;
}

EditorDirtyState TranslateGizmo::draw(SceneGraph& sceneGraph, const RenderCamera& camera, ImVec2 imageMin, ImVec2 imageMax)
{
    EditorDirtyState dirty;
    SdfGraph& graph = sceneGraph.graph();
    SdfGraphNodeId selectedId = graph.selectedNode();
    SdfGraphNode* selected = graph.node(selectedId);
    if (selected == nullptr || (selected->payload.type != SdfNodeType::Translate && !isPrimitiveNode(selected->payload.type))) {
        m_activeAxis = -1;
        return dirty;
    }

    SdfGraphNodeId gizmoNodeId = selectedId;
    SdfGraphNode* gizmoNode = selected;
    if (selected->payload.type != SdfNodeType::Translate) {
        if (const SdfGraphNodeId translateId = GraphSystem::findDirectTranslateParent(graph, selectedId)) {
            if (SdfGraphNode* translate = graph.node(translateId)) {
                gizmoNodeId = translateId;
                gizmoNode = translate;
            }
        }
    }

    glm::vec3 origin = gizmoNode->payload.type == SdfNodeType::Translate ? GraphSystem::accumulatedTranslatePosition(graph, gizmoNodeId) : glm::vec3{0.0f, 0.0f, 0.0f};
    const std::optional<ImVec2> originScreen = projectWorldToScreen(origin, camera, imageMin, imageMax);
    if (!originScreen) {
        return dirty;
    }

    const glm::vec3 axes[] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };
    const ImU32 colors[] = {
        IM_COL32(235, 70, 70, 255),
        IM_COL32(80, 210, 100, 255),
        IM_COL32(80, 130, 240, 255),
    };

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    std::optional<ImVec2> axisEnds[3];
    int hoveredAxis = -1;
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    for (int axis = 0; axis < 3; ++axis) {
        axisEnds[axis] = projectWorldToScreen(origin + axes[axis] * AXIS_LENGTH, camera, imageMin, imageMax);
        if (axisEnds[axis] && hovered && distancePointToSegment(mouse, *originScreen, *axisEnds[axis]) <= HIT_DISTANCE) {
            hoveredAxis = axis;
        }
    }

    for (int axis = 0; axis < 3; ++axis) {
        if (!axisEnds[axis]) {
            continue;
        }
        const bool activeAxis = m_activeAxis == axis;
        const bool hot = hoveredAxis == axis;
        const float thickness = activeAxis ? ACTIVE_LINE_THICKNESS : (hot ? HOVER_LINE_THICKNESS : LINE_THICKNESS);
        drawList->AddLine(*originScreen, *axisEnds[axis], colors[axis], thickness);
        drawList->AddCircleFilled(*axisEnds[axis], activeAxis || hot ? 7.0f : 5.0f, colors[axis]);
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredAxis >= 0) {
        if (gizmoNode->payload.type == SdfNodeType::Translate) {
            selectedId = gizmoNodeId;
            selected = gizmoNode;
            graph.setSelectedNode(selectedId);
        } else {
            selectedId = GraphSystem::ensureTranslateWrapperForNode(graph, selectedId);
            selected = graph.node(selectedId);
            dirty.scene = true;
            if (selected == nullptr) {
                return dirty;
            }
        }
        origin = GraphSystem::accumulatedTranslatePosition(graph, selectedId);
        m_activeAxis = hoveredAxis;
        m_dragWorldOrigin = origin;
        m_dragStartLocalPosition = translatePosition(*selected);
        m_dragStartAxisT = axisParameterFromMouse(mouse, m_dragWorldOrigin, axes[m_activeAxis], camera, imageMin, imageMax).value_or(0.0f);
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        m_activeAxis = -1;
    }

    if (m_activeAxis >= 0 && selected != nullptr && selected->payload.type == SdfNodeType::Translate) {
        const std::optional<float> currentAxisT = axisParameterFromMouse(mouse, m_dragWorldOrigin, axes[m_activeAxis], camera, imageMin, imageMax);
        if (currentAxisT) {
            const glm::vec3 moved = m_dragStartLocalPosition + axes[m_activeAxis] * (*currentAxisT - m_dragStartAxisT);
            selected->payload.parameters["x"] = moved.x;
            selected->payload.parameters["y"] = moved.y;
            selected->payload.parameters["z"] = moved.z;
            dirty.scene = true;
        }
    }

    return dirty;
}

} // namespace sdf3d
