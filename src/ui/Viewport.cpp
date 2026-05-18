#include "sdf3d/ui/Viewport.h"

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

} // namespace

EditorDirtyState Viewport::draw(Renderer& renderer, SceneGraph& sceneGraph)
{
    EditorDirtyState dirty;
    ImGui::Begin("Viewport");

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const int width = std::max(1, static_cast<int>(available.x));
    const int height = std::max(1, static_cast<int>(available.y));
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    handleInput(hovered, {available.x, available.y});

    renderer.resize(width, height);
    renderer.render(camera());

    // AGENT: Rendering to a texture keeps the raymarched viewport inside the
    // dockable ImGui panel instead of fighting the main framebuffer clear.
    const ImTextureID textureId = static_cast<ImTextureID>(static_cast<intptr_t>(renderer.outputTexture()));
    ImGui::Image(textureId, available, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    const ImVec2 imageMin = ImGui::GetItemRectMin();
    const ImVec2 imageMax = ImGui::GetItemRectMax();
    const RenderCamera renderCamera = camera();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
        && ImGui::IsMouseReleased(ImGuiMouseButton_Right)
        && !m_rightMouseMoved) {
        m_pendingAddWorldPosition = viewportSpawnPosition(ImGui::GetIO().MousePos, renderCamera, imageMin, imageMax);
        ImGui::OpenPopup(node_editor::NODE_ADD_POPUP_ID);
    }
    if (m_viewportAddMenu.drawViewportPopup(sceneGraph, m_pendingAddWorldPosition)) {
        dirty.scene = true;
    }

    const EditorDirtyState gizmoDirty = m_translateGizmo.draw(sceneGraph, renderCamera, imageMin, imageMax);
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
    (void)panelSize;
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
        orbit(delta);
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
