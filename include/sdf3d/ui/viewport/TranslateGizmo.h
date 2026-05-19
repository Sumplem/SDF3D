#pragma once

#include "sdf3d/renderer/UniformUploader.h"
#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/ui/EditorDirtyState.h"

#include <glm/glm.hpp>
#include <imgui.h>

namespace sdf3d {

class TranslateGizmo {
public:
    EditorDirtyState update(SceneGraph& sceneGraph, const RenderCamera& camera, ImVec2 imageMin, ImVec2 imageMax, RenderGizmo& gizmo);
    void drawSettings();
    bool active() const;

private:
    enum class Mode {
        Translate,
        Rotate,
        Scale,
    };

    glm::vec3 m_dragWorldOrigin = {0.0f, 0.0f, 0.0f};
    glm::mat3 m_dragOrientation = glm::mat3{1.0f};
    glm::vec3 m_dragStartLocalPosition = {0.0f, 0.0f, 0.0f};
    glm::vec4 m_dragStartQuaternion = {0.0f, 0.0f, 0.0f, 1.0f};
    float m_dragStartScale = 1.0f;
    float m_dragStartAxisT = 0.0f;
    float m_dragStartAngle = 0.0f;
    int m_activeAxis = -1;
    Mode m_mode = Mode::Translate;
    GizmoRotateStyle m_rotateStyle = GizmoRotateStyle::Rings;
};

} // namespace sdf3d
