#pragma once

#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/systems/DiagnosticsSystem.h"
#include "sdf3d/systems/SelectionSystem.h"
#include "sdf3d/ui/AddMenu.h"
#include "sdf3d/ui/NodeEditor.h"
#include "sdf3d/ui/PropertiesPanel.h"
#include "sdf3d/ui/SceneOutliner.h"

#include <string>
#include <vector>

namespace sdf3d {

/// Coordinates editor UI panels and scene edit dirty state.
class UI {
public:
    /// Draws menu entries that can mutate the scene.
    void drawMainMenu(SceneGraph& sceneGraph);

    /// Draws the scene outliner and properties panel.
    void drawPanels(SceneGraph& sceneGraph, const std::vector<DiagnosticEntry>& runtimeErrors = {});

    /// Returns true once when a scene edit requires shader recompilation.
    bool consumeSceneDirty();

private:
    void drawScenePanel(SceneGraph& sceneGraph);
    void drawDiagnosticsPanel(const std::vector<DiagnosticEntry>& runtimeErrors);
    void markSceneDirty();

    SelectionSystem m_selectionSystem;
    AddMenu m_addMenu;
    NodeEditor m_nodeEditor;
    PropertiesPanel m_propertiesPanel;
    SceneOutliner m_sceneOutliner;
};

} // namespace sdf3d
