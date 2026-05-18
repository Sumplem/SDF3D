#pragma once

#include "sdf3d/renderer/UniformUploader.h"
#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/ui/EditorDirtyState.h"

#include <glm/glm.hpp>
#include <imgui.h>

namespace sdf3d {

class TranslateGizmo {
public:
    EditorDirtyState draw(SceneGraph& sceneGraph, const RenderCamera& camera, ImVec2 imageMin, ImVec2 imageMax);
    bool active() const;

private:
    glm::vec3 m_dragWorldOrigin = {0.0f, 0.0f, 0.0f};
    glm::vec3 m_dragStartLocalPosition = {0.0f, 0.0f, 0.0f};
    float m_dragStartAxisT = 0.0f;
    int m_activeAxis = -1;
};

} // namespace sdf3d
