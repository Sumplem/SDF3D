#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace sdf3d::node_editor {
namespace {

struct LinkInsertionCandidate {
    SdfGraphLink link;
    std::string inputSocket;
    std::string outputSocket;
};

ImVec2 cubicBezierPoint(ImVec2 p0, ImVec2 p1, ImVec2 p2, ImVec2 p3, float t)
{
    const float u = 1.0f - t;
    const float tt = t * t;
    const float uu = u * u;
    const float uuu = uu * u;
    const float ttt = tt * t;
    return {
        uuu * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x + ttt * p3.x,
        uuu * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y + ttt * p3.y,
    };
}

bool pointInsideRect(ImVec2 point, ImVec2 min, ImVec2 max)
{
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
}

bool rectOverBezier(const GraphNodeLayout& layout, ImVec2 from, ImVec2 to)
{
    constexpr int sampleCount = 24;
    const float zoom = std::max(0.01f, layout.size.x / NODE_WIDTH);
    const ImVec2 min = {layout.position.x - 8.0f * zoom, layout.position.y - 8.0f * zoom};
    const ImVec2 max = {layout.position.x + layout.size.x + 8.0f * zoom, layout.position.y + layout.size.y + 8.0f * zoom};
    const ImVec2 c1 = {from.x + 70.0f * zoom, from.y};
    const ImVec2 c2 = {to.x - 70.0f * zoom, to.y};
    for (int i = 0; i <= sampleCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleCount);
        if (pointInsideRect(cubicBezierPoint(from, c1, c2, to, t), min, max)) {
            return true;
        }
    }

    return false;
}

std::optional<LinkInsertionCandidate> findLinkInsertionCandidate(
    const SdfGraph& graph,
    const GraphNodeLayout& layout,
    const std::vector<GraphSocketAnchor>& anchors)
{
    const SdfGraphNode* dragged = graph.node(layout.id);
    if (dragged == nullptr || graph.isOutputNode(layout.id) || graph.hasLinks(layout.id)) {
        return std::nullopt;
    }

    for (const SdfGraphLink& link : graph.links()) {
        if (link.fromNode == layout.id || link.toNode == layout.id) {
            continue;
        }

        const std::optional<ImVec2> from = findSocketAnchor(anchors, link.fromNode, link.fromSocket, true);
        const std::optional<ImVec2> to = findSocketAnchor(anchors, link.toNode, link.toSocket, false);
        if (!from || !to || !rectOverBezier(layout, *from, *to)) {
            continue;
        }

        for (const SdfGraphSocket& input : dragged->inputs) {
            if (!socketsCompatible(graph, link.fromNode, link.fromSocket, layout.id, input.name)) {
                continue;
            }
            for (const SdfGraphSocket& output : dragged->outputs) {
                if (socketsCompatible(graph, layout.id, output.name, link.toNode, link.toSocket)) {
                    return LinkInsertionCandidate{link, input.name, output.name};
                }
            }
        }
    }

    return std::nullopt;
}

} // namespace

std::optional<ImVec2> findSocketAnchor(
    const std::vector<GraphSocketAnchor>& anchors,
    SdfGraphNodeId node,
    const std::string& socket,
    bool output)
{
    for (const GraphSocketAnchor& anchor : anchors) {
        if (anchor.node == node && anchor.socket == socket && anchor.output == output) {
            return anchor.position;
        }
    }

    return std::nullopt;
}

const SdfGraphSocket* findSocket(const std::vector<SdfGraphSocket>& sockets, const std::string& name, SdfSocketDirection direction)
{
    for (const SdfGraphSocket& socket : sockets) {
        if (socket.name == name && socket.direction == direction) {
            return &socket;
        }
    }

    return nullptr;
}

bool socketsCompatible(const SdfGraph& graph, SdfGraphNodeId fromNode, const std::string& fromSocket, SdfGraphNodeId toNode, const std::string& toSocket)
{
    const SdfGraphNode* from = graph.node(fromNode);
    const SdfGraphNode* to = graph.node(toNode);
    if (from == nullptr || to == nullptr || fromNode == toNode) {
        return false;
    }

    const SdfGraphSocket* output = findSocket(from->outputs, fromSocket, SdfSocketDirection::Output);
    const SdfGraphSocket* input = findSocket(to->inputs, toSocket, SdfSocketDirection::Input);
    return output != nullptr && input != nullptr && output->type == input->type;
}

bool activeOutputNodeExists(const SdfGraph& graph)
{
    const SdfGraphNode* outputNode = graph.node(graph.outputNode());
    return outputNode != nullptr && outputNode->payload.type == SdfNodeType::Output;
}

float distanceSquared(ImVec2 a, ImVec2 b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    return x * x + y * y;
}

std::optional<SdfGraphLink> linkToInput(const SdfGraph& graph, SdfGraphNodeId node, const std::string& socket)
{
    for (const SdfGraphLink& link : graph.links()) {
        if (link.toNode == node && link.toSocket == socket) {
            return link;
        }
    }

    return std::nullopt;
}

bool previewSelectedNode(SdfGraph& graph)
{
    const SdfGraphNodeId selectedNode = graph.selectedNode();
    const SdfGraphNodeId outputNode = graph.outputNode();
    const SdfGraphNode* selected = graph.node(selectedNode);
    const SdfGraphNode* output = graph.node(outputNode);
    if (selected == nullptr
        || output == nullptr
        || selectedNode == outputNode
        || output->payload.type != SdfNodeType::Output
        || findSocket(selected->outputs, "sdf", SdfSocketDirection::Output) == nullptr
        || findSocket(output->inputs, "surface", SdfSocketDirection::Input) == nullptr) {
        return false;
    }

    if (const std::optional<SdfGraphLink> existing = linkToInput(graph, outputNode, "surface")) {
        if (existing->fromNode == selectedNode && existing->fromSocket == "sdf") {
            return false;
        }
    }

    // AGENT: Preview shortcut only rewires the fixed Output surface input;
    // graph output ownership remains the non-deletable Output node.
    return graph.link(selectedNode, "sdf", outputNode, "surface");
}

std::optional<SdfGraphLink> firstLinkFromOutput(const SdfGraph& graph, SdfGraphNodeId node, const std::string& socket)
{
    for (const SdfGraphLink& link : graph.links()) {
        if (link.fromNode == node && link.fromSocket == socket) {
            return link;
        }
    }

    return std::nullopt;
}

bool insertNodeIntoLink(SdfGraph& graph, const GraphNodeLayout& layout, const std::vector<GraphSocketAnchor>& anchors)
{
    const std::optional<LinkInsertionCandidate> candidate = findLinkInsertionCandidate(graph, layout, anchors);
    if (!candidate) {
        return false;
    }

    // AGENT: Insert rewires one existing edge into A -> dragged -> B only
    // after both socket directions have already been compatibility-checked.
    graph.unlink(candidate->link.fromNode, candidate->link.fromSocket, candidate->link.toNode, candidate->link.toSocket);
    const bool linkedInput = graph.link(candidate->link.fromNode, candidate->link.fromSocket, layout.id, candidate->inputSocket);
    const bool linkedOutput = graph.link(layout.id, candidate->outputSocket, candidate->link.toNode, candidate->link.toSocket);
    if (!linkedInput || !linkedOutput) {
        graph.unlink(candidate->link.fromNode, candidate->link.fromSocket, layout.id, candidate->inputSocket);
        graph.unlink(layout.id, candidate->outputSocket, candidate->link.toNode, candidate->link.toSocket);
        graph.link(candidate->link.fromNode, candidate->link.fromSocket, candidate->link.toNode, candidate->link.toSocket);
        return false;
    }

    return true;
}

void drawLinkInsertionPreview(
    const SdfGraph& graph,
    const GraphNodeLayout& layout,
    const CanvasFrame& frame,
    const std::vector<GraphSocketAnchor>& anchors)
{
    const std::optional<LinkInsertionCandidate> candidate = findLinkInsertionCandidate(graph, layout, anchors);
    if (!candidate) {
        return;
    }

    const std::optional<ImVec2> originalFrom = findSocketAnchor(anchors, candidate->link.fromNode, candidate->link.fromSocket, true);
    const std::optional<ImVec2> originalTo = findSocketAnchor(anchors, candidate->link.toNode, candidate->link.toSocket, false);
    const std::optional<ImVec2> insertedInput = findSocketAnchor(anchors, layout.id, candidate->inputSocket, false);
    const std::optional<ImVec2> insertedOutput = findSocketAnchor(anchors, layout.id, candidate->outputSocket, true);
    if (!originalFrom || !originalTo || !insertedInput || !insertedOutput) {
        return;
    }

    const ImU32 previewWire = IM_COL32(255, 210, 110, 235);
    const ImU32 dimWire = IM_COL32(120, 126, 140, 130);
    const float handle = scaleValue(frame, 70.0f);
    frame.drawList->AddBezierCubic(*originalFrom, {originalFrom->x + handle, originalFrom->y}, {originalTo->x - handle, originalTo->y}, *originalTo, dimWire, scaleValue(frame, 5.0f));
    frame.drawList->AddBezierCubic(*originalFrom, {originalFrom->x + handle, originalFrom->y}, {insertedInput->x - handle, insertedInput->y}, *insertedInput, previewWire, scaleValue(frame, 3.0f));
    frame.drawList->AddBezierCubic(*insertedOutput, {insertedOutput->x + handle, insertedOutput->y}, {originalTo->x - handle, originalTo->y}, *originalTo, previewWire, scaleValue(frame, 3.0f));

    const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};
    frame.drawList->AddRect(layout.position, nodeEnd, previewWire, scaleValue(frame, 6.0f), 0, scaleValue(frame, 3.0f));
    frame.drawList->AddCircleFilled(*insertedInput, scaleValue(frame, 7.0f), previewWire);
    frame.drawList->AddCircleFilled(*insertedOutput, scaleValue(frame, 7.0f), previewWire);
}

bool mouseNearBezier(ImVec2 mouse, ImVec2 from, ImVec2 to)
{
    constexpr int sampleCount = 24;
    constexpr float hitRadiusSquared = 64.0f;
    const ImVec2 c1 = {from.x + 70.0f, from.y};
    const ImVec2 c2 = {to.x - 70.0f, to.y};
    for (int i = 0; i <= sampleCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleCount);
        if (distanceSquared(mouse, cubicBezierPoint(from, c1, c2, to, t)) <= hitRadiusSquared) {
            return true;
        }
    }

    return false;
}

EditorDirtyState updateNodeEditorLinkDrags(
    SdfGraph& graph,
    const CanvasFrame& frame,
    const std::vector<GraphSocketAnchor>& anchors,
    NodeEditorDragState& drag,
    LinkDragResult& result)
{
    EditorDirtyState dirty;
    result.releasedFromNode = drag.dragOutputNode;
    result.releasedFromSocket = drag.dragOutputSocket;
    const bool suppressOutputPopup = drag.dragOutputFromInputDetach;
    result.releasedToNode = drag.dragInputNode;
    result.releasedToSocket = drag.dragInputSocket;

    if (updateActiveLinkDrag(graph, frame, anchors, drag.draggingLink, drag.dragOutputNode, drag.dragOutputSocket, result.releasedLinkOnEmpty)) {
        dirty.scene = true;
    }
    if (!drag.draggingLink) {
        drag.dragOutputFromInputDetach = false;
        if (suppressOutputPopup) {
            if (result.releasedLinkOnEmpty && drag.detachedInputNode != 0 && !drag.detachedInputSocket.empty()) {
                result.releasedInputLinkOnEmpty = true;
                result.releasedToNode = drag.detachedInputNode;
                result.releasedToSocket = drag.detachedInputSocket;
            }
            result.releasedLinkOnEmpty = false;
            drag.detachedInputNode = 0;
            drag.detachedInputSocket.clear();
        }
    }

    if (updateActiveInputLinkDrag(
            graph,
            frame,
            anchors,
            drag.draggingInputLink,
            drag.dragInputNode,
            drag.dragInputSocket,
            drag.dragInputAnchorOffsetY,
            result.releasedInputLinkOnEmpty)) {
        dirty.scene = true;
    }

    return dirty;
}

} // namespace sdf3d::node_editor
