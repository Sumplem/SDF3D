#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <optional>

namespace sdf3d::node_editor {

bool updateActiveInputLinkDrag(
    SdfGraph& graph,
    const CanvasFrame& frame,
    const std::vector<GraphSocketAnchor>& anchors,
    bool& draggingInputLink,
    SdfGraphNodeId& dragInputNode,
    std::string& dragInputSocket,
    float& dragInputAnchorOffsetY,
    bool& releasedOnEmpty)
{
    releasedOnEmpty = false;
    if (!draggingInputLink) {
        return false;
    }

    if (std::optional<ImVec2> to = findSocketAnchor(anchors, dragInputNode, dragInputSocket, false)) {
        to->y += dragInputAnchorOffsetY;
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float handle = scaleValue(frame, 70.0f);
        frame.drawList->AddBezierCubic(*to, {to->x - handle, to->y}, {mouse.x + handle, mouse.y}, mouse, IM_COL32(255, 210, 110, 255), scaleValue(frame, 3.0f));
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        return false;
    }

    bool sceneDirty = false;
    bool linkedTarget = false;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float pinHitRadius = scaleValue(frame, 12.0f);
    for (const GraphSocketAnchor& anchor : anchors) {
        if (!anchor.output || distanceSquared(anchor.position, mouse) > pinHitRadius * pinHitRadius) {
            continue;
        }

        if (socketsCompatible(graph, anchor.node, anchor.socket, dragInputNode, dragInputSocket)
            && graph.link(anchor.node, anchor.socket, dragInputNode, dragInputSocket)) {
            graph.setSelectedNode(dragInputNode);
            sceneDirty = true;
            linkedTarget = true;
            break;
        }
    }

    releasedOnEmpty = !linkedTarget && ImGui::IsWindowHovered();
    draggingInputLink = false;
    dragInputNode = 0;
    dragInputSocket.clear();
    dragInputAnchorOffsetY = 0.0f;
    return sceneDirty;
}

} // namespace sdf3d::node_editor
