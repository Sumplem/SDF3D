#pragma once

#include "sdf3d/renderer/Renderer.h"
#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/ui/AddMenu.h"
#include "sdf3d/ui/EditorDirtyState.h"
#include "sdf3d/ui/viewport/TranslateGizmo.h"

#include <glm/glm.hpp>
#include <imgui.h>

namespace sdf3d {

class GraphGroupRegistry;

/// ImGui viewport panel with a basic orbit camera for M2.
class Viewport {
public:
    Viewport() = default;

    /// Draws the viewport panel and renders the raymarched scene into it.
    EditorDirtyState draw(Renderer& renderer, SdfGraph& graph, const GraphGroupRegistry& groups);

    /// Returns the current camera values for rendering.
    RenderCamera camera() const;

private:
    void handleInput(bool hovered, const glm::vec2& panelSize);
    void orbit(const glm::vec2& mouseDelta);
    void pan(const glm::vec2& mouseDelta, const glm::vec2& panelSize);
    void zoom(float wheelDelta);
    void updateCameraPosition();

    glm::vec3 m_target = {0.0f, 0.0f, 0.0f};
    float m_distance = 4.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_fovDegrees = 45.0f;
    glm::vec2 m_lastMouse = {0.0f, 0.0f};
    glm::vec3 m_position = {0.0f, 0.0f, 4.0f};
    RenderQuality m_quality = RenderQuality::High;
    RenderMode m_renderMode = RenderMode::DirectPreview;
    TranslateGizmo m_translateGizmo;
    AddMenu m_viewportAddMenu;
    glm::vec3 m_pendingAddWorldPosition = {0.0f, 0.0f, 0.0f};
    int m_hoverNodeId = -1;
    bool m_dragging = false;
    bool m_rightMouseMoved = false;
    bool m_showHoverHighlight = true;
};

} // namespace sdf3d
