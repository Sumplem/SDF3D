#include "sdf3d/ui/NodeEditor.h"

#include "sdf3d/scene/SdfNodeDefinition.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

namespace sdf3d {
namespace {

constexpr float NODE_WIDTH = 220.0f;
constexpr float TITLE_HEIGHT = 28.0f;
constexpr float SOCKET_ROW_HEIGHT = 22.0f;
constexpr float PIN_HIT_RADIUS_SQUARED = 144.0f;

struct GraphNodeLayout {
    SdfGraphNodeId id = 0;
    SdfGraphNode* node = nullptr;
    ImVec2 position = {0.0f, 0.0f};
    ImVec2 size = {0.0f, 0.0f};
};

struct GraphSocketAnchor {
    SdfGraphNodeId node = 0;
    std::string socket;
    bool output = false;
    ImVec2 position = {0.0f, 0.0f};
};

struct CanvasFrame {
    ImVec2 origin = {0.0f, 0.0f};
    ImVec2 end = {0.0f, 0.0f};
    ImDrawList* drawList = nullptr;
};

struct LinkInsertionCandidate {
    SdfGraphLink link;
    std::string inputSocket;
    std::string outputSocket;
};

const char* graphNodeTypeName(SdfNodeType type)
{
    if (const SdfNodeDefinition* definition = sdfNodeDefinition(type)) {
        return definition->displayName.c_str();
    }

    return "Node";
}

const char* socketTypeName(SdfSocketType type)
{
    switch (type) {
    case SdfSocketType::Sdf:
        return "SDF";
    case SdfSocketType::Float:
        return "Float";
    case SdfSocketType::Vector3:
        return "Vector3";
    case SdfSocketType::Material:
        return "Material";
    }

    return "Unknown";
}

std::string socketDisplayName(const SdfGraphSocket& socket)
{
    std::string label = socket.name;
    label += " : ";
    label += socketTypeName(socket.type);
    return label;
}

std::string graphNodeDisplayName(const SdfGraphNode* node)
{
    if (node == nullptr) {
        return "None";
    }

    std::string label = node->payload.name.empty() ? graphNodeTypeName(node->payload.type) : node->payload.name;
    label += " #";
    label += std::to_string(node->id);
    return label;
}

std::vector<SdfGraphNodeId> sortedNodeIds(const SdfGraph& graph)
{
    std::vector<SdfGraphNodeId> ids;
    ids.reserve(graph.nodes().size());
    for (const auto& [id, node] : graph.nodes()) {
        (void)node;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

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

bool pointInsideRect(ImVec2 point, ImVec2 min, ImVec2 max)
{
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
}

bool rectOverBezier(const GraphNodeLayout& layout, ImVec2 from, ImVec2 to)
{
    constexpr int sampleCount = 24;
    const ImVec2 min = {layout.position.x - 8.0f, layout.position.y - 8.0f};
    const ImVec2 max = {layout.position.x + layout.size.x + 8.0f, layout.position.y + layout.size.y + 8.0f};
    const ImVec2 c1 = {from.x + 70.0f, from.y};
    const ImVec2 c2 = {to.x - 70.0f, to.y};
    for (int i = 0; i <= sampleCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleCount);
        if (pointInsideRect(cubicBezierPoint(from, c1, c2, to, t), min, max)) {
            return true;
        }
    }

    return false;
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

std::optional<SdfGraphLink> firstLinkFromOutput(const SdfGraph& graph, SdfGraphNodeId node, const std::string& socket)
{
    for (const SdfGraphLink& link : graph.links()) {
        if (link.fromNode == node && link.fromSocket == socket) {
            return link;
        }
    }

    return std::nullopt;
}

bool isBypassInput(SdfNodeType type, const std::string& socket)
{
    switch (type) {
    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion:
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect:
        return socket == "left" || socket == "right";
    case SdfNodeType::Subtract:
    case SdfNodeType::SmoothSubtract:
        return socket == "base";
    default:
        return false;
    }
}

bool nodeHasMissingRequiredInput(const SdfGraph& graph, const SdfGraphNode& node)
{
    for (const SdfGraphSocket& input : node.inputs) {
        if (input.type == SdfSocketType::Sdf && !linkToInput(graph, node.id, input.name)) {
            return true;
        }
    }

    return false;
}

std::optional<SdfGraphLink> bypassSourceLink(const SdfGraph& graph, const SdfGraphNode& node)
{
    if (!nodeHasMissingRequiredInput(graph, node)) {
        return std::nullopt;
    }

    for (const SdfGraphSocket& input : node.inputs) {
        if (input.type != SdfSocketType::Sdf || !isBypassInput(node.payload.type, input.name)) {
            continue;
        }

        if (const std::optional<SdfGraphLink> link = linkToInput(graph, node.id, input.name)) {
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
    frame.drawList->AddBezierCubic(*originalFrom, {originalFrom->x + 70.0f, originalFrom->y}, {originalTo->x - 70.0f, originalTo->y}, *originalTo, dimWire, 5.0f);
    frame.drawList->AddBezierCubic(*originalFrom, {originalFrom->x + 70.0f, originalFrom->y}, {insertedInput->x - 70.0f, insertedInput->y}, *insertedInput, previewWire, 3.0f);
    frame.drawList->AddBezierCubic(*insertedOutput, {insertedOutput->x + 70.0f, insertedOutput->y}, {originalTo->x - 70.0f, originalTo->y}, *originalTo, previewWire, 3.0f);

    const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};
    frame.drawList->AddRect(layout.position, nodeEnd, previewWire, 6.0f, 0, 3.0f);
    frame.drawList->AddCircleFilled(*insertedInput, 7.0f, previewWire);
    frame.drawList->AddCircleFilled(*insertedOutput, 7.0f, previewWire);
}

void drawInactiveNodePreview(
    const SdfGraph& graph,
    const GraphNodeLayout& layout,
    const CanvasFrame& frame,
    const std::vector<GraphSocketAnchor>& anchors)
{
    const SdfGraphNode& node = *layout.node;
    if (!nodeHasMissingRequiredInput(graph, node)) {
        return;
    }

    const ImU32 inactiveColor = IM_COL32(255, 180, 80, 210);
    const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};
    frame.drawList->AddRect(layout.position, nodeEnd, inactiveColor, 6.0f, 0, 2.0f);

    for (const SdfGraphSocket& input : node.inputs) {
        if (input.type != SdfSocketType::Sdf || linkToInput(graph, layout.id, input.name)) {
            continue;
        }

        if (const std::optional<ImVec2> pin = findSocketAnchor(anchors, layout.id, input.name, false)) {
            frame.drawList->AddCircle(*pin, 8.0f, inactiveColor, 16, 2.0f);
        }
    }

    const std::optional<SdfGraphLink> source = bypassSourceLink(graph, node);
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
    frame.drawList->AddBezierCubic(*from, {from->x + 90.0f, from->y}, {to->x - 90.0f, to->y}, *to, IM_COL32(255, 180, 80, 180), 2.0f);
}

CanvasFrame beginCanvas()
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 canvasSize = {std::max(available.x, 320.0f), std::max(available.y, 260.0f)};
    ImGui::BeginChild("GraphCanvas", canvasSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    CanvasFrame frame;
    frame.origin = ImGui::GetCursorScreenPos();
    frame.end = {frame.origin.x + canvasSize.x, frame.origin.y + canvasSize.y};
    frame.drawList = ImGui::GetWindowDrawList();
    return frame;
}

void drawGrid(const CanvasFrame& frame)
{
    frame.drawList->AddRectFilled(frame.origin, frame.end, IM_COL32(30, 32, 36, 255));

    constexpr float gridStep = 32.0f;
    for (float x = frame.origin.x; x < frame.end.x; x += gridStep) {
        frame.drawList->AddLine({x, frame.origin.y}, {x, frame.end.y}, IM_COL32(48, 50, 56, 255));
    }
    for (float y = frame.origin.y; y < frame.end.y; y += gridStep) {
        frame.drawList->AddLine({frame.origin.x, y}, {frame.end.x, y}, IM_COL32(48, 50, 56, 255));
    }
}

void buildLayoutsAndAnchors(
    SdfGraph& graph,
    const CanvasFrame& frame,
    std::vector<GraphNodeLayout>& layouts,
    std::vector<GraphSocketAnchor>& anchors)
{
    const std::vector<SdfGraphNodeId> ids = sortedNodeIds(graph);
    layouts.reserve(ids.size());

    for (size_t index = 0; index < ids.size(); ++index) {
        SdfGraphNode* node = graph.node(ids[index]);
        if (node == nullptr) {
            continue;
        }

        if (node->editorX == 0.0f && node->editorY == 0.0f && ids[index] != 1) {
            node->editorX = 40.0f + static_cast<float>(index % 3) * 260.0f;
            node->editorY = 40.0f + static_cast<float>(index / 3) * 150.0f;
        }

        const size_t socketRows = std::max(node->inputs.size(), node->outputs.size());
        const float nodeHeight = TITLE_HEIGHT + 34.0f + std::max<size_t>(1, socketRows) * SOCKET_ROW_HEIGHT;
        const ImVec2 nodePosition = {frame.origin.x + 24.0f + node->editorX, frame.origin.y + 24.0f + node->editorY};
        layouts.push_back({ids[index], node, nodePosition, {NODE_WIDTH, nodeHeight}});

        for (size_t i = 0; i < node->inputs.size(); ++i) {
            anchors.push_back({ids[index], node->inputs[i].name, false, {nodePosition.x, nodePosition.y + TITLE_HEIGHT + 18.0f + static_cast<float>(i) * SOCKET_ROW_HEIGHT}});
        }
        for (size_t i = 0; i < node->outputs.size(); ++i) {
            anchors.push_back({ids[index], node->outputs[i].name, true, {nodePosition.x + NODE_WIDTH, nodePosition.y + TITLE_HEIGHT + 18.0f + static_cast<float>(i) * SOCKET_ROW_HEIGHT}});
        }
    }
}

bool drawExistingLinks(SdfGraph& graph, const CanvasFrame& frame, const std::vector<GraphSocketAnchor>& anchors)
{
    std::optional<SdfGraphLink> pendingRemoval;
    for (const SdfGraphLink& link : graph.links()) {
        const std::optional<ImVec2> from = findSocketAnchor(anchors, link.fromNode, link.fromSocket, true);
        const std::optional<ImVec2> to = findSocketAnchor(anchors, link.toNode, link.toSocket, false);
        if (!from || !to) {
            continue;
        }

        frame.drawList->AddBezierCubic(*from, {from->x + 70.0f, from->y}, {to->x - 70.0f, to->y}, *to, IM_COL32(130, 170, 255, 255), 3.0f);
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && mouseNearBezier(ImGui::GetIO().MousePos, *from, *to)) {
            pendingRemoval = link;
        }
    }

    return pendingRemoval && graph.unlink(pendingRemoval->fromNode, pendingRemoval->fromSocket, pendingRemoval->toNode, pendingRemoval->toSocket);
}

void drawNodeBody(SdfGraph& graph, const GraphNodeLayout& layout, const CanvasFrame& frame)
{
    SdfGraphNode& node = *layout.node;
    const bool selected = graph.selectedNode() == layout.id;
    const bool output = graph.outputNode() == layout.id;
    const ImU32 bodyColor = selected ? IM_COL32(58, 66, 84, 255) : IM_COL32(42, 45, 52, 255);
    const ImU32 titleColor = node.payload.type == SdfNodeType::Output ? IM_COL32(96, 74, 48, 255) : (output ? IM_COL32(76, 96, 70, 255) : IM_COL32(54, 58, 68, 255));
    const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};

    frame.drawList->AddRectFilled(layout.position, nodeEnd, bodyColor, 6.0f);
    frame.drawList->AddRectFilled(layout.position, {nodeEnd.x, layout.position.y + TITLE_HEIGHT}, titleColor, 6.0f, ImDrawFlags_RoundCornersTop);
    frame.drawList->AddRect(layout.position, nodeEnd, selected ? IM_COL32(120, 170, 255, 255) : IM_COL32(78, 82, 92, 255), 6.0f, 0, selected ? 2.0f : 1.0f);

    const std::string title = graphNodeDisplayName(&node) + (output ? "  [Output]" : "");
    frame.drawList->AddText({layout.position.x + 10.0f, layout.position.y + 7.0f}, IM_COL32(235, 238, 242, 255), title.c_str());
}

bool handleNodeTitleDrag(SdfGraph& graph, const GraphNodeLayout& layout, SdfGraphNodeId& activeDraggedNode)
{
    static SdfGraphNodeId draggedNode = 0;
    bool releasedDraggedNode = false;
    ImGui::SetCursorScreenPos(layout.position);
    ImGui::InvisibleButton(("node-title##" + std::to_string(layout.id)).c_str(), {layout.size.x, TITLE_HEIGHT});
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        graph.setSelectedNode(layout.id);
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        layout.node->editorX += delta.x;
        layout.node->editorY += delta.y;
        draggedNode = layout.id;
        activeDraggedNode = layout.id;
    }
    if (ImGui::IsItemDeactivated() && draggedNode == layout.id) {
        releasedDraggedNode = true;
        draggedNode = 0;
    }

    return releasedDraggedNode;
}

bool drawInputPins(
    SdfGraph& graph,
    const GraphNodeLayout& layout,
    const CanvasFrame& frame,
    bool& draggingLink,
    SdfGraphNodeId& dragOutputNode,
    std::string& dragOutputSocket,
    SdfGraphNodeId& inputDragCandidateNode,
    std::string& inputDragCandidateSocket)
{
    bool sceneDirty = false;
    SdfGraphNode& node = *layout.node;

    for (size_t i = 0; i < node.inputs.size(); ++i) {
        const SdfGraphSocket& socket = node.inputs[i];
        const ImVec2 pin = {layout.position.x, layout.position.y + TITLE_HEIGHT + 18.0f + static_cast<float>(i) * SOCKET_ROW_HEIGHT};
        const bool dragHover = draggingLink
            && distanceSquared(pin, ImGui::GetIO().MousePos) <= PIN_HIT_RADIUS_SQUARED
            && socketsCompatible(graph, dragOutputNode, dragOutputSocket, layout.id, socket.name);

        frame.drawList->AddCircleFilled(pin, dragHover ? 7.0f : 5.0f, dragHover ? IM_COL32(255, 210, 110, 255) : IM_COL32(120, 180, 120, 255));
        frame.drawList->AddText({pin.x + 10.0f, pin.y - 7.0f}, IM_COL32(220, 224, 230, 255), socketDisplayName(socket).c_str());

        ImGui::SetCursorScreenPos({pin.x - 8.0f, pin.y - 8.0f});
        ImGui::InvisibleButton(("input##" + std::to_string(layout.id) + "-" + socket.name).c_str(), {16.0f, 16.0f});
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            inputDragCandidateNode = layout.id;
            inputDragCandidateSocket = socket.name;
            graph.setSelectedNode(layout.id);
        }
        if (ImGui::IsItemActive()
            && ImGui::IsMouseDragging(ImGuiMouseButton_Left)
            && !draggingLink
            && inputDragCandidateNode == layout.id
            && inputDragCandidateSocket == socket.name) {
            if (const std::optional<SdfGraphLink> existing = linkToInput(graph, layout.id, socket.name)) {
                if (graph.unlink(existing->fromNode, existing->fromSocket, existing->toNode, existing->toSocket)) {
                    draggingLink = true;
                    dragOutputNode = existing->fromNode;
                    dragOutputSocket = existing->fromSocket;
                    sceneDirty = true;
                }
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && inputDragCandidateNode == layout.id && inputDragCandidateSocket == socket.name) {
            inputDragCandidateNode = 0;
            inputDragCandidateSocket.clear();
        }
    }

    return sceneDirty;
}

void drawOutputPins(
    SdfGraph& graph,
    const GraphNodeLayout& layout,
    const CanvasFrame& frame,
    bool& draggingLink,
    SdfGraphNodeId& dragOutputNode,
    std::string& dragOutputSocket)
{
    SdfGraphNode& node = *layout.node;

    for (size_t i = 0; i < node.outputs.size(); ++i) {
        const SdfGraphSocket& socket = node.outputs[i];
        const ImVec2 pin = {layout.position.x + layout.size.x, layout.position.y + TITLE_HEIGHT + 18.0f + static_cast<float>(i) * SOCKET_ROW_HEIGHT};
        const bool activeDrag = draggingLink && dragOutputNode == layout.id && dragOutputSocket == socket.name;
        frame.drawList->AddCircleFilled(pin, activeDrag ? 7.0f : 5.0f, activeDrag ? IM_COL32(255, 210, 110, 255) : IM_COL32(120, 160, 240, 255));

        const std::string label = socketDisplayName(socket);
        const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        frame.drawList->AddText({pin.x - 10.0f - labelSize.x, pin.y - 7.0f}, IM_COL32(220, 224, 230, 255), label.c_str());

        ImGui::SetCursorScreenPos({pin.x - 8.0f, pin.y - 8.0f});
        ImGui::InvisibleButton(("output##" + std::to_string(layout.id) + "-" + socket.name).c_str(), {16.0f, 16.0f});
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            draggingLink = true;
            dragOutputNode = layout.id;
            dragOutputSocket = socket.name;
            graph.setSelectedNode(layout.id);
        }
    }
}

bool drawNodeActions(SdfGraph& graph, const GraphNodeLayout& layout, SdfGraphNodeId& pendingDelete)
{
    const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};

    ImGui::SetCursorScreenPos({layout.position.x + 10.0f, nodeEnd.y - 26.0f});
    if (graph.isOutputNode(layout.id)) {
        ImGui::BeginDisabled();
        ImGui::SmallButton(("Delete##" + std::to_string(layout.id)).c_str());
        ImGui::EndDisabled();
    } else if (ImGui::SmallButton(("Delete##" + std::to_string(layout.id)).c_str())) {
        pendingDelete = layout.id;
    }

    return false;
}

bool updateActiveLinkDrag(
    SdfGraph& graph,
    const CanvasFrame& frame,
    const std::vector<GraphSocketAnchor>& anchors,
    bool& draggingLink,
    SdfGraphNodeId& dragOutputNode,
    std::string& dragOutputSocket)
{
    if (!draggingLink) {
        return false;
    }

    if (const std::optional<ImVec2> from = findSocketAnchor(anchors, dragOutputNode, dragOutputSocket, true)) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        frame.drawList->AddBezierCubic(*from, {from->x + 70.0f, from->y}, {mouse.x - 70.0f, mouse.y}, mouse, IM_COL32(255, 210, 110, 255), 3.0f);
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        return false;
    }

    bool sceneDirty = false;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    for (const GraphSocketAnchor& anchor : anchors) {
        if (anchor.output || distanceSquared(anchor.position, mouse) > PIN_HIT_RADIUS_SQUARED) {
            continue;
        }

        if (socketsCompatible(graph, dragOutputNode, dragOutputSocket, anchor.node, anchor.socket)
            && graph.link(dragOutputNode, dragOutputSocket, anchor.node, anchor.socket)) {
            graph.setSelectedNode(anchor.node);
            const SdfGraphNode* target = graph.node(anchor.node);
            if (target != nullptr && (target->payload.type == SdfNodeType::Output || !activeOutputNodeExists(graph))) {
                graph.setOutputNode(anchor.node);
            }
            sceneDirty = true;
            break;
        }
    }

    draggingLink = false;
    dragOutputNode = 0;
    dragOutputSocket.clear();
    return sceneDirty;
}

} // namespace

bool NodeEditor::draw(SceneGraph& sceneGraph)
{
    bool sceneDirty = false;
    SdfGraph& graph = sceneGraph.graph();

    const CanvasFrame frame = beginCanvas();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_P)) {
        if (previewSelectedNode(graph)) {
            sceneDirty = true;
        }
    }

    drawGrid(frame);

    std::vector<GraphNodeLayout> layouts;
    std::vector<GraphSocketAnchor> anchors;
    buildLayoutsAndAnchors(graph, frame, layouts, anchors);

    if (drawExistingLinks(graph, frame, anchors)) {
        sceneDirty = true;
    }

    SdfGraphNodeId pendingDelete = 0;
    if (layouts.empty()) {
        frame.drawList->AddText({frame.origin.x + 16.0f, frame.origin.y + 16.0f}, IM_COL32(210, 215, 225, 255), "Empty graph");
    }

    SdfGraphNodeId releasedDraggedNode = 0;
    SdfGraphNodeId activeDraggedNode = 0;
    for (const GraphNodeLayout& layout : layouts) {
        drawNodeBody(graph, layout, frame);
        drawInactiveNodePreview(graph, layout, frame, anchors);
        if (handleNodeTitleDrag(graph, layout, activeDraggedNode)) {
            releasedDraggedNode = layout.id;
        }
        if (drawInputPins(graph, layout, frame, m_draggingLink, m_dragOutputNode, m_dragOutputSocket, m_inputDragCandidateNode, m_inputDragCandidateSocket)) {
            sceneDirty = true;
        }
        drawOutputPins(graph, layout, frame, m_draggingLink, m_dragOutputNode, m_dragOutputSocket);
        if (drawNodeActions(graph, layout, pendingDelete)) {
            sceneDirty = true;
        }
    }

    if (activeDraggedNode != 0) {
        for (const GraphNodeLayout& layout : layouts) {
            if (layout.id == activeDraggedNode) {
                drawLinkInsertionPreview(graph, layout, frame, anchors);
                break;
            }
        }
    }

    if (releasedDraggedNode != 0) {
        for (const GraphNodeLayout& layout : layouts) {
            if (layout.id == releasedDraggedNode && insertNodeIntoLink(graph, layout, anchors)) {
                sceneDirty = true;
                break;
            }
        }
    }

    if (updateActiveLinkDrag(graph, frame, anchors, m_draggingLink, m_dragOutputNode, m_dragOutputSocket)) {
        sceneDirty = true;
    }

    if (pendingDelete != 0) {
        graph.deleteNode(pendingDelete);
        sceneDirty = true;
    }

    ImGui::EndChild();
    return sceneDirty;
}

} // namespace sdf3d
