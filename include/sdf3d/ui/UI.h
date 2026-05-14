#pragma once

#include "sdf3d/scene/SceneGraph.h"

namespace sdf3d {

/// Coordinates editor UI panels and scene edit dirty state.
class UI {
public:
    /// Draws menu entries that can mutate the scene.
    void drawMainMenu(SceneGraph& sceneGraph);

    /// Draws the scene outliner and properties panel.
    void drawPanels(SceneGraph& sceneGraph);

    /// Returns true once when a scene edit requires shader recompilation.
    bool consumeSceneDirty();

private:
    void drawAddMenu(SceneGraph& sceneGraph);
    void drawScenePanel(SceneGraph& sceneGraph);
    void drawProperties(SceneGraph& sceneGraph);
    void addPrimitive(SceneGraph& sceneGraph, SdfNodePtr node);
    void markSceneDirty();

    bool m_sceneDirty = false;
};

} // namespace sdf3d
