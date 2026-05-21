#include "sdf3d/ui/UI.h"

#include "sdf3d/core/EventBus.h"

#include <imgui.h>

namespace sdf3d {
namespace {

constexpr const char* DEFAULT_GRAPH_PATH = "sdf3d_graph.json";

} // namespace

void UI::setEventBus(EventBus* eventBus)
{
    m_eventBus = eventBus;
    m_nodeEditor.setEventBus(eventBus);
}

void UI::drawMainMenu(SceneGraph& sceneGraph, GraphGroupRegistry& groups)
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Save Graph")) {
            if (m_eventBus != nullptr) {
                m_eventBus->emit(SaveGraphEvent{DEFAULT_GRAPH_PATH});
            }
        }
        if (ImGui::MenuItem("Load Graph")) {
            if (m_eventBus != nullptr) {
                m_eventBus->emit(LoadGraphEvent{DEFAULT_GRAPH_PATH});
            }
        }
        ImGui::Separator();
        ImGui::MenuItem("Exit");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        ImGui::EndMenu();
    }

    if (m_addMenu.draw(activeGraph(sceneGraph, groups))) {
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

namespace {

const char* severityName(DiagnosticSeverity severity)
{
    switch (severity) {
    case DiagnosticSeverity::Info:
        return "Info";
    case DiagnosticSeverity::Warning:
        return "Warning";
    case DiagnosticSeverity::Error:
        return "Error";
    }

    return "Unknown";
}

ImVec4 severityColor(DiagnosticSeverity severity)
{
    switch (severity) {
    case DiagnosticSeverity::Info:
        return {0.45f, 0.82f, 0.55f, 1.0f};
    case DiagnosticSeverity::Warning:
        return {0.95f, 0.70f, 0.30f, 1.0f};
    case DiagnosticSeverity::Error:
        return {0.95f, 0.35f, 0.35f, 1.0f};
    }

    return {0.80f, 0.80f, 0.80f, 1.0f};
}

} // namespace

void UI::drawPanels(SceneGraph& sceneGraph, GraphGroupRegistry& groups, const std::vector<DiagnosticEntry>& runtimeErrors)
{
    drawScenePanel(sceneGraph, groups);
    drawDiagnosticsPanel(runtimeErrors);
    const EditorDirtyState propertiesDirty = m_propertiesPanel.draw(activeGraph(sceneGraph, groups), groups);
    if (propertiesDirty.scene) {
        markSceneDirty();
    }
    if (propertiesDirty.material) {
        markMaterialDirty();
    }
}

SdfGraph& UI::activeGraph(SceneGraph& sceneGraph, GraphGroupRegistry& groups)
{
    return m_nodeEditor.activeGraph(sceneGraph, groups);
}

void UI::resetActiveGraph()
{
    m_nodeEditor.resetActiveGraph();
}

bool UI::consumeSceneDirty()
{
    return m_selectionSystem.consumeDirty();
}

bool UI::consumeMaterialDirty()
{
    return m_selectionSystem.consumeMaterialDirty();
}

void UI::drawScenePanel(SceneGraph& sceneGraph, GraphGroupRegistry& groups)
{
    ImGui::Begin("Scene");
    SdfGraph& graph = activeGraph(sceneGraph, groups);
    ImGui::TextDisabled("Scope: %s", m_nodeEditor.activeGroupName(groups));

    if (ImGui::GetCurrentContext() != nullptr) {
        if (m_sceneOutliner.draw(graph, groups)) {
            markSceneDirty();
        }

        const EditorDirtyState nodeEditorDirty = m_nodeEditor.draw(sceneGraph, groups);
        if (nodeEditorDirty.scene) {
            markSceneDirty();
        }
        if (nodeEditorDirty.material) {
            markMaterialDirty();
        }

        // AGENT: Runtime branch keeps old text-list graph UI compiled as
        // fallback during this migration, but normal editor path is canvas.
        ImGui::End();
        return;
    }

    if (m_sceneOutliner.draw(graph, groups)) {
        markSceneDirty();
    }

    // AGENT: The editor has moved to graph-first scene editing; legacy tree UI
    // remains compiled for migration helpers but is no longer displayed.
    ImGui::End();
}

void UI::drawDiagnosticsPanel(const std::vector<DiagnosticEntry>& runtimeErrors)
{
    ImGui::Begin("Diagnostics");
    if (runtimeErrors.empty()) {
        ImGui::TextColored(severityColor(DiagnosticSeverity::Info), "Shader validation passed.");
        ImGui::End();
        return;
    }

    int infoCount = 0;
    int warningCount = 0;
    int errorCount = 0;
    for (const DiagnosticEntry& entry : runtimeErrors) {
        switch (entry.severity) {
        case DiagnosticSeverity::Info:
            ++infoCount;
            break;
        case DiagnosticSeverity::Warning:
            ++warningCount;
            break;
        case DiagnosticSeverity::Error:
            ++errorCount;
            break;
        }
    }

    const DiagnosticSeverity statusSeverity = errorCount > 0 ? DiagnosticSeverity::Error : (warningCount > 0 ? DiagnosticSeverity::Warning : DiagnosticSeverity::Info);
    const char* statusText = errorCount > 0 ? "Shader validation failed" : (warningCount > 0 ? "Shader validation has warnings" : "Shader validation passed");
    ImGui::TextColored(severityColor(statusSeverity), "%s", statusText);
    ImGui::Text("Errors %d  Warnings %d  Info %d", errorCount, warningCount, infoCount);
    ImGui::Separator();

    for (const DiagnosticEntry& entry : runtimeErrors) {
        ImGui::TextColored(severityColor(entry.severity), "%s", severityName(entry.severity));
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", entry.source.empty() ? "Runtime" : entry.source.c_str());
        ImGui::TextWrapped("%s", entry.message.c_str());
        ImGui::Separator();
    }
    ImGui::End();
}

void UI::markSceneDirty()
{
    m_selectionSystem.markDirty();
}

void UI::markMaterialDirty()
{
    m_selectionSystem.markMaterialDirty();
}

} // namespace sdf3d
