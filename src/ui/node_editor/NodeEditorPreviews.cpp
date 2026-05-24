#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include "sdf3d/systems/GraphSystem.h"

#include <optional>

namespace sdf3d::node_editor {

void drawInactiveNodePreview(
    const SdfGraph& graph,
    const GraphNodeLayout& layout,
    const CanvasFrame& frame,
    const std::vector<GraphSocketAnchor>& anchors)
{
    const SdfGraphNode& node = *layout.node;
    if (!GraphSystem::nodeHasMissingRequiredInput(graph, node)) {
        return;
    }

    const ImU32 inactiveColor = IM_COL32(255, 180, 80, 210);
    const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};
    frame.drawList->AddRect(layout.position, nodeEnd, inactiveColor, scaleValue(frame, 6.0f), 0, scaleValue(frame, 2.0f));

    for (const SdfGraphSocket& input : node.inputs) {
        if (input.type != SdfSocketType::Sdf || !GraphSystem::effectiveLinksToInput(graph, layout.id, input.name).empty()) {
            continue;
        }

        if (const std::optional<ImVec2> pin = findSocketAnchor(anchors, layout.id, input.name, false)) {
            frame.drawList->AddCircle(*pin, scaleValue(frame, 8.0f), inactiveColor, 16, scaleValue(frame, 2.0f));
        }
    }

    const std::optional<SdfGraphLink> source = GraphSystem::effectiveBypassSourceLink(graph, node);
    const std::optional<SdfGraphLink> downstream = firstLinkFromOutput(graph, layout.id, "sdf");
    if (!source || !downstream) {
        return;
    }

    const std::optional<ImVec2> from = findSocketAnchor(anchors, source->fromNode, source->fromSocket, true);
    const std::optional<ImVec2> to = findSocketAnchor(anchors, downstream->toNode, downstream->toSocket, false);
    if (!from || !to) {
        return;
    }

    // AGENT: Compiler bypasses incomplete boolean nodes in these cases; this
    // virtual wire shows that effective path without mutating graph links.
    ui::drawGraphBezier(frame, *from, *to, IM_COL32(255, 180, 80, 180), 2.0f, 90.0f);
}

} // namespace sdf3d::node_editor
