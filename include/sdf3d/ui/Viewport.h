#pragma once

#include "sdf3d/renderer/Renderer.h"

#include <glm/glm.hpp>

namespace sdf3d {

/// ImGui viewport panel with a basic orbit camera for M2.
class Viewport {
public:
    Viewport() = default;

    /// Draws the viewport panel and renders the raymarched scene into it.
    void draw(Renderer& renderer);

    /// Returns the current camera values for rendering.
    RenderCamera camera() const;

private:
    void handleInput(bool hovered, const glm::vec2& panelSize);
    void orbit(const glm::vec2& mouseDelta);
    void zoom(float wheelDelta);
    void updateCameraPosition();

    glm::vec3 m_target = {0.0f, 0.0f, 0.0f};
    float m_distance = 4.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_fovDegrees = 45.0f;
    glm::vec2 m_lastMouse = {0.0f, 0.0f};
    glm::vec3 m_position = {0.0f, 0.0f, 4.0f};
    bool m_dragging = false;
};

} // namespace sdf3d
