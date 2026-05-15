#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <algorithm>
#include <vector>

namespace sdf3d::node_editor {
namespace {

float layoutZoom(const GraphNodeLayout& layout)
{
    return std::max(0.01f, layout.size.x / NODE_WIDTH);
}

bool isPrimitiveNode(SdfNodeType type)
{
    return type == SdfNodeType::Sphere
        || type == SdfNodeType::Box
        || type == SdfNodeType::Cylinder
        || type == SdfNodeType::Torus
        || type == SdfNodeType::Plane
        || type == SdfNodeType::Capsule
        || type == SdfNodeType::Cone
        || type == SdfNodeType::RoundBox;
}

bool wrapInMaterialOverride(SdfGraph& graph, const GraphNodeLayout& layout)
{
    const SdfGraphNodeId materialNode = graph.createNode(SdfNodeType::MaterialOverride);
    SdfGraphNode* material = graph.node(materialNode);
    if (material == nullptr) {
        return false;
    }

    material->editorX = layout.node->editorX + NODE_WIDTH + 40.0f;
    material->editorY = layout.node->editorY;

    const std::vector<SdfGraphLink> oldLinks = graph.links();
    for (const SdfGraphLink& link : oldLinks) {
        if (link.fromNode == layout.id && link.fromSocket == "sdf") {
            graph.unlink(link.fromNode, link.fromSocket, link.toNode, link.toSocket);
            graph.link(materialNode, "sdf", link.toNode, link.toSocket);
        }
    }

    // AGENT: Wrap keeps existing downstream links, then inserts material tag
    // between primitive geometry and all consumers.
    graph.link(layout.id, "sdf", materialNode, "sdf");
    graph.setSelectedNode(materialNode);
    return true;
}

} // namespace

bool drawNodeActions(SdfGraph& graph, const GraphNodeLayout& layout, SdfGraphNodeId& pendingDelete)
{
    bool sceneDirty = false;
    const float zoom = layoutZoom(layout);
    const float buttonExtent = std::max(16.0f, 18.0f * zoom);
    const ImVec2 buttonSize = {buttonExtent, buttonExtent};
    const float top = layout.position.y + 5.0f * zoom;
    const float deleteX = layout.position.x + layout.size.x - buttonExtent - 5.0f * zoom;
    const float collapseX = deleteX - buttonExtent - 4.0f * zoom;

    ImGui::SetCursorScreenPos({collapseX, top});
    const char* collapseLabel = layout.node->editorPropertiesCollapsed ? "+##node-props-" : "-##node-props-";
    if (ImGui::Button((std::string(collapseLabel) + std::to_string(layout.id)).c_str(), buttonSize)) {
        layout.node->editorPropertiesCollapsed = !layout.node->editorPropertiesCollapsed;
    }

    ImGui::SetCursorScreenPos({deleteX, top});
    if (graph.isOutputNode(layout.id)) {
        ImGui::BeginDisabled();
        ImGui::Button(("X##delete-node-" + std::to_string(layout.id)).c_str(), buttonSize);
        ImGui::EndDisabled();
    } else if (ImGui::Button(("X##delete-node-" + std::to_string(layout.id)).c_str(), buttonSize)) {
        pendingDelete = layout.id;
    }

    const std::string popupId = "node-context-" + std::to_string(layout.id);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};
    const bool mouseInsideNode = mouse.x >= layout.position.x && mouse.x <= nodeEnd.x && mouse.y >= layout.position.y && mouse.y <= nodeEnd.y;
    if (isPrimitiveNode(layout.node->payload.type)
        && mouseInsideNode
        && ImGui::IsWindowHovered()
        && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        graph.setSelectedNode(layout.id);
        ImGui::OpenPopup(popupId.c_str());
    }

    if (ImGui::BeginPopup(popupId.c_str())) {
        if (ImGui::MenuItem("Wrap in Material Override")) {
            sceneDirty = wrapInMaterialOverride(graph, layout);
        }
        ImGui::EndPopup();
    }

    return sceneDirty;
}

} // namespace sdf3d::node_editor
