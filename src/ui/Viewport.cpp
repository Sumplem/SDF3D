#include "sdf3d/ui/Viewport.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <imgui.h>

namespace sdf3d {
namespace {

constexpr float PI = 3.14159265358979323846f;

float radians(float degrees)
{
    return degrees * PI / 180.0f;
}

} // namespace

void Viewport::draw(Renderer& renderer)
{
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
    ImGui::End();
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

    ImGuiIO& io = ImGui::GetIO();
    const glm::vec2 mouse = {io.MousePos.x, io.MousePos.y};

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        m_dragging = true;
        m_lastMouse = mouse;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        m_dragging = false;
    }

    if (m_dragging) {
        const glm::vec2 delta = mouse - m_lastMouse;
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
