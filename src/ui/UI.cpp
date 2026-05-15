#include "sdf3d/ui/UI.h"

#include <imgui.h>

namespace sdf3d {

void UI::drawMainMenu(SceneGraph& sceneGraph)
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        ImGui::MenuItem("Exit");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        ImGui::EndMenu();
    }

    if (m_addMenu.draw(sceneGraph)) {
        markSceneDirty();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void UI::drawPanels(SceneGraph& sceneGraph, const std::vector<std::string>& runtimeErrors)
{
    drawScenePanel(sceneGraph);
    drawDiagnosticsPanel(runtimeErrors);
    if (m_propertiesPanel.draw(sceneGraph)) {
        markSceneDirty();
    }
}

bool UI::consumeSceneDirty()
{
    const bool dirty = m_sceneDirty;
    m_sceneDirty = false;
    return dirty;
}

void UI::drawScenePanel(SceneGraph& sceneGraph)
{
    ImGui::Begin("Scene");

    if (ImGui::GetCurrentContext() != nullptr) {
        if (m_sceneOutliner.draw(sceneGraph)) {
            markSceneDirty();
        }

        if (m_nodeEditor.draw(sceneGraph)) {
            markSceneDirty();
        }

        // AGENT: Runtime branch keeps old text-list graph UI compiled as
        // fallback during this migration, but normal editor path is canvas.
        ImGui::End();
        return;
    }

    if (m_sceneOutliner.draw(sceneGraph)) {
        markSceneDirty();
    }

    // AGENT: The editor has moved to graph-first scene editing; legacy tree UI
    // remains compiled for migration helpers but is no longer displayed.
    ImGui::End();
}

void UI::drawDiagnosticsPanel(const std::vector<std::string>& runtimeErrors)
{
    ImGui::Begin("Diagnostics");
    if (runtimeErrors.empty()) {
        ImGui::TextUnformatted("No runtime errors.");
        ImGui::End();
        return;
    }

    for (const std::string& error : runtimeErrors) {
        ImGui::TextWrapped("%s", error.c_str());
        ImGui::Separator();
    }
    ImGui::End();
}

void UI::markSceneDirty()
{
    m_sceneDirty = true;
}

} // namespace sdf3d
