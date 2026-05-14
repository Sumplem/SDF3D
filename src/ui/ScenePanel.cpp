#include "sdf3d/ui/ScenePanel.h"

#include <imgui.h>

namespace sdf3d {
namespace {

const char* fallbackNodeName(SdfNodeType type)
{
    switch (type) {
    case SdfNodeType::Sphere:
        return "Sphere";
    case SdfNodeType::Box:
        return "Box";
    case SdfNodeType::Cylinder:
        return "Cylinder";
    case SdfNodeType::Torus:
        return "Torus";
    case SdfNodeType::Plane:
        return "Plane";
    case SdfNodeType::Union:
        return "Union";
    case SdfNodeType::SmoothUnion:
        return "Smooth Union";
    case SdfNodeType::Subtract:
        return "Subtract";
    case SdfNodeType::SmoothSubtract:
        return "Smooth Subtract";
    case SdfNodeType::Intersect:
        return "Intersect";
    case SdfNodeType::SmoothIntersect:
        return "Smooth Intersect";
    case SdfNodeType::Translate:
        return "Translate";
    case SdfNodeType::Rotate:
        return "Rotate";
    case SdfNodeType::Scale:
        return "Scale";
    default:
        return "Node";
    }
}

} // namespace

void ScenePanel::draw(SceneGraph& sceneGraph)
{
    if (sceneGraph.root()) {
        drawNode(sceneGraph.root(), sceneGraph);
    } else {
        ImGui::TextUnformatted("Empty scene");
    }
}

void ScenePanel::drawNode(const SdfNodePtr& node, SceneGraph& sceneGraph)
{
    if (!node) {
        return;
    }

    const bool selected = node == sceneGraph.selectedNode();
    const bool leaf = node->children.empty();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_OpenOnDoubleClick
        | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (leaf) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    const char* label = node->name.empty() ? fallbackNodeName(node->type) : node->name.c_str();
    // AGENT: The pointer ID is stable for the node lifetime and avoids requiring
    // persistent node IDs before serialization support exists.
    const bool open = ImGui::TreeNodeEx(static_cast<void*>(node.get()), flags, "%s", label);

    if (ImGui::IsItemClicked()) {
        sceneGraph.setSelectedNode(node);
    }

    if (open && !leaf) {
        for (const SdfNodePtr& child : node->children) {
            drawNode(child, sceneGraph);
        }
        ImGui::TreePop();
    }
}

} // namespace sdf3d
