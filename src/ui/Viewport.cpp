#include "sdf3d/ui/Viewport.h"

#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/systems/GraphSystem.h"
#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

namespace sdf3d {
namespace {

constexpr float PI = 3.14159265358979323846f;

float radians(float degrees)
{
    return degrees * PI / 180.0f;
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

glm::vec3 viewportSpawnPosition(ImVec2 mouse, const RenderCamera& camera, ImVec2 imageMin, ImVec2 imageMax)
{
    const std::optional<glm::vec3> direction = screenRayDirection(mouse, camera, imageMin, imageMax);
    if (!direction || std::abs(direction->y) <= 0.0001f) {
        return camera.target;
    }

    const float t = -camera.position.y / direction->y;
    if (t <= 0.0f) {
        return camera.target;
    }
    return camera.position + *direction * t;
}

std::optional<glm::ivec2> nodeIdPixelForMouse(ImVec2 mouse, ImVec2 imageMin, ImVec2 imageMax, int width, int height)
{
    if (mouse.x < imageMin.x || mouse.x > imageMax.x || mouse.y < imageMin.y || mouse.y > imageMax.y) {
        return std::nullopt;
    }

    const float imageWidth = std::max(1.0f, imageMax.x - imageMin.x);
    const float imageHeight = std::max(1.0f, imageMax.y - imageMin.y);
    const int x = std::clamp(static_cast<int>(((mouse.x - imageMin.x) / imageWidth) * static_cast<float>(width)), 0, width - 1);
    const int yFromTop = std::clamp(static_cast<int>(((mouse.y - imageMin.y) / imageHeight) * static_cast<float>(height)), 0, height - 1);
    return glm::ivec2{x, height - 1 - yFromTop};
}

} // namespace

EditorDirtyState Viewport::draw(Renderer& renderer, SdfGraph& graph, const GraphGroupRegistry& groups)
{
    EditorDirtyState dirty;
    ImGui::Begin("Viewport");

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const int width = std::max(1, static_cast<int>(available.x));
    const int height = std::max(1, static_cast<int>(available.y));
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    handleInput(hovered, {available.x, available.y});

    const ImVec2 imageMin = ImGui::GetCursorScreenPos();
    const ImVec2 imageMax = {imageMin.x + available.x, imageMin.y + available.y};
    const RenderCamera renderCamera = camera();
    RenderGizmo gizmo;
    const EditorDirtyState gizmoDirty = m_translateGizmo.update(graph, renderCamera, imageMin, imageMax, gizmo);
    gizmo.highlightNodeId = static_cast<int>(GraphSystem::highlightNodeForSelection(graph));

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool imageHovered = hovered
        && mouse.x >= imageMin.x
        && mouse.x <= imageMax.x
        && mouse.y >= imageMin.y
        && mouse.y <= imageMax.y;
    const bool canPickScene = imageHovered
        && !m_translateGizmo.active()
        && gizmo.hoverAxis < 0
        && !m_dragging;
    const std::optional<glm::ivec2> pickPixel = nodeIdPixelForMouse(mouse, imageMin, imageMax, width, height);
    int nodeIdUnderMouse = -1;
    if (canPickScene && pickPixel) {
        nodeIdUnderMouse = renderer.readNodeIdPixel(pickPixel->x, pickPixel->y);
    }
    if (canPickScene && pickPixel && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (nodeIdUnderMouse > 0) {
            graph.setSelectedNode(static_cast<SdfGraphNodeId>(nodeIdUnderMouse));
        } else {
            graph.clearSelection();
        }
    }
    m_hoverNodeId = m_showHoverHighlight && canPickScene && nodeIdUnderMouse > 0
        ? static_cast<int>(GraphSystem::highlightNodeForNode(graph, static_cast<SdfGraphNodeId>(nodeIdUnderMouse)))
        : -1;
    gizmo.hoverNodeId = m_hoverNodeId;

    if (gizmoDirty.params) {
        renderer.setNodeParams(GraphSystem::collectNodeParams(graph, groups));
        dirty.params = true;
    }

    renderer.setGizmo(gizmo);
    renderer.setQuality(m_quality);
    renderer.setRenderMode(m_renderMode);

    renderer.resize(width, height);
    renderer.render(renderCamera);

    // AGENT: Rendering to a texture keeps the raymarched viewport inside the
    // dockable ImGui panel instead of fighting the main framebuffer clear.
    const ImTextureID textureId = static_cast<ImTextureID>(static_cast<intptr_t>(renderer.outputTexture()));
    ImGui::Image(textureId, available, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    ImGui::SetCursorScreenPos({imageMin.x + 8.0f, imageMin.y + 8.0f});
    m_translateGizmo.drawSettings();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(92.0f);
    int quality = static_cast<int>(m_quality);
    const char* qualityLabels[] = {"Low", "Medium", "High"};
    if (ImGui::Combo("Quality", &quality, qualityLabels, 3)) {
        m_quality = static_cast<RenderQuality>(quality);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(136.0f);
    int renderMode = static_cast<int>(m_renderMode);
    const char* modeLabels[] = {"Direct", "Path Trace"};
    if (ImGui::Combo("Mode", &renderMode, modeLabels, 2)) {
        m_renderMode = static_cast<RenderMode>(renderMode);
    }
    if (m_renderMode == RenderMode::ProgressivePathTrace) {
        ImGui::SameLine();
        if (renderer.pathTraceActive()) {
            ImGui::Text("Samples %u", renderer.pathTraceSampleCount());
        } else {
            ImGui::TextUnformatted("Direct preview");
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Show hover highlight", &m_showHoverHighlight);
    if (!m_showHoverHighlight) {
        m_hoverNodeId = -1;
    }

    if (imageHovered
        && ImGui::IsMouseReleased(ImGuiMouseButton_Right)
        && !m_rightMouseMoved) {
        m_pendingAddWorldPosition = viewportSpawnPosition(ImGui::GetIO().MousePos, renderCamera, imageMin, imageMax);
        ImGui::OpenPopup(node_editor::NODE_ADD_POPUP_ID);
    }
    if (m_viewportAddMenu.drawViewportPopup(graph, m_pendingAddWorldPosition)) {
        dirty.scene = true;
    }

    dirty.scene = dirty.scene || gizmoDirty.scene;
    ImGui::End();
    return dirty;
}

RenderCamera Viewport::camera() const
{
    RenderCamera renderCamera;
    renderCamera.position = m_position;
    renderCamera.target = m_target;
    renderCamera.up = {0.0f, 1.0f, 0.0f};
    renderCamera.fovDegrees = m_fovDegrees;
    return renderCamera;
}

void Viewport::handleInput(bool hovered, const glm::vec2& panelSize)
{
    if (m_translateGizmo.active()) {
        updateCameraPosition();
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const glm::vec2 mouse = {io.MousePos.x, io.MousePos.y};

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        m_dragging = true;
        m_rightMouseMoved = false;
        m_lastMouse = mouse;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        m_dragging = false;
    }

    if (m_dragging) {
        const glm::vec2 delta = mouse - m_lastMouse;
        if (glm::length(delta) > 0.5f) {
            m_rightMouseMoved = true;
        }
        if (io.KeyShift) {
            pan(delta, panelSize);
        } else {
            orbit(delta);
        }
        m_lastMouse = mouse;
    }

    if (hovered && io.MouseWheel != 0.0f) {
        zoom(io.MouseWheel);
    }

    updateCameraPosition();
}

void Viewport::orbit(const glm::vec2& mouseDelta)
{
    constexpr float orbitSpeed = 0.005f;
    m_yaw -= mouseDelta.x * orbitSpeed;
    m_pitch -= mouseDelta.y * orbitSpeed;

    const float pitchLimit = radians(89.0f);
    m_pitch = std::clamp(m_pitch, -pitchLimit, pitchLimit);
}

void Viewport::pan(const glm::vec2& mouseDelta, const glm::vec2& panelSize)
{
    const float height = std::max(1.0f, panelSize.y);
    const float worldUnitsPerPixel = (2.0f * m_distance * std::tan(radians(m_fovDegrees) * 0.5f)) / height;

    const glm::vec3 worldUp = {0.0f, 1.0f, 0.0f};
    const glm::vec3 forward = glm::normalize(m_target - m_position);
    glm::vec3 right = glm::cross(forward, worldUp);
    if (glm::length(right) <= 0.0001f) {
        right = {1.0f, 0.0f, 0.0f};
    } else {
        right = glm::normalize(right);
    }
    const glm::vec3 up = glm::normalize(glm::cross(right, forward));

    // AGENT: Pan moves orbit target in camera plane; orbit angles and distance stay unchanged.
    const glm::vec3 offset = ((-right * mouseDelta.x) + (up * mouseDelta.y)) * worldUnitsPerPixel;
    m_target += offset;
    m_position += offset;
}

void Viewport::zoom(float wheelDelta)
{
    constexpr float zoomStep = 0.85f;
    if (wheelDelta > 0.0f) {
        m_distance *= zoomStep;
    } else {
        m_distance /= zoomStep;
    }

    m_distance = std::clamp(m_distance, 1.0f, 25.0f);
}

void Viewport::updateCameraPosition()
{
    const float cosPitch = std::cos(m_pitch);
    const glm::vec3 direction = {
        std::sin(m_yaw) * cosPitch,
        std::sin(m_pitch),
        std::cos(m_yaw) * cosPitch,
    };

    m_position = m_target + direction * m_distance;
}

} // namespace sdf3d
