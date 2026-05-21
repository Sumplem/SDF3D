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

class EventBus;
class GraphGroupRegistry;

/// Coordinates editor UI panels and scene edit dirty state.
class UI {
public:
    void setEventBus(EventBus* eventBus);

    /// Draws menu entries that can mutate the active graph.
    void drawMainMenu(SceneGraph& sceneGraph, GraphGroupRegistry& groups);

    /// Draws the scene outliner and properties panel.
    void drawPanels(SceneGraph& sceneGraph, GraphGroupRegistry& groups, const std::vector<DiagnosticEntry>& runtimeErrors = {});

    /// Returns graph currently edited by graph UI.
    SdfGraph& activeGraph(SceneGraph& sceneGraph, GraphGroupRegistry& groups);

    /// Resets graph UI navigation to root graph.
    void resetActiveGraph();

    /// Returns true once when a scene edit requires shader recompilation.
    bool consumeSceneDirty();

    /// Returns true once when a material edit requires uniform upload.
    bool consumeMaterialDirty();

private:
    void drawScenePanel(SceneGraph& sceneGraph, GraphGroupRegistry& groups);
    void drawDiagnosticsPanel(const std::vector<DiagnosticEntry>& runtimeErrors);
    void markSceneDirty();
    void markMaterialDirty();

    EventBus* m_eventBus = nullptr;
    SelectionSystem m_selectionSystem;
    AddMenu m_addMenu;
    NodeEditor m_nodeEditor;
    PropertiesPanel m_propertiesPanel;
    SceneOutliner m_sceneOutliner;
};

} // namespace sdf3d
