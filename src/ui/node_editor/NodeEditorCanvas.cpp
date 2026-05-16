#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include "sdf3d/scene/SdfNodeDefinition.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace sdf3d::node_editor {
namespace {

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

size_t inlinePropertyRows(const SdfGraphNode& node)
{
    if (node.editorPropertiesCollapsed) {
        return 0;
    }

    constexpr size_t nameRows = 1;
    const size_t materialRows = node.payload.type == SdfNodeType::MaterialOverride ? 4 : 0;
    return nameRows + materialRows + node.payload.parameters.size();
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

float layoutZoom(const GraphNodeLayout& layout)
{
    return std::max(0.01f, layout.size.x / NODE_WIDTH);
}

void addScaledText(const CanvasFrame& frame, ImVec2 position, ImU32 color, const char* text)
{
    frame.drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * frame.zoom, position, color, text);
}

} // namespace

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
        const float nodeHeight = TITLE_HEIGHT + 34.0f + std::max<size_t>(1, socketRows) * SOCKET_ROW_HEIGHT + static_cast<float>(inlinePropertyRows(*node)) * 24.0f;
        const ImVec2 nodePosition = graphToScreen(frame, {24.0f + node->editorX, 24.0f + node->editorY});
        const ImVec2 contentPosition = {nodePosition.x + scaleValue(frame, 10.0f), nodePosition.y + scaleValue(frame, TITLE_HEIGHT + 24.0f + static_cast<float>(socketRows) * SOCKET_ROW_HEIGHT)};
        layouts.push_back({ids[index], node, nodePosition, {scaleValue(frame, NODE_WIDTH), scaleValue(frame, nodeHeight)}, contentPosition});

        for (size_t i = 0; i < node->inputs.size(); ++i) {
            anchors.push_back({ids[index], node->inputs[i].name, false, {nodePosition.x, nodePosition.y + scaleValue(frame, TITLE_HEIGHT + 18.0f + static_cast<float>(i) * SOCKET_ROW_HEIGHT)}});
        }
        for (size_t i = 0; i < node->outputs.size(); ++i) {
            anchors.push_back({ids[index], node->outputs[i].name, true, {nodePosition.x + scaleValue(frame, NODE_WIDTH), nodePosition.y + scaleValue(frame, TITLE_HEIGHT + 18.0f + static_cast<float>(i) * SOCKET_ROW_HEIGHT)}});
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

        const float handle = scaleValue(frame, 70.0f);
        frame.drawList->AddBezierCubic(*from, {from->x + handle, from->y}, {to->x - handle, to->y}, *to, IM_COL32(130, 170, 255, 255), scaleValue(frame, 3.0f));
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && mouseNearBezier(ImGui::GetIO().MousePos, *from, *to)) {
            pendingRemoval = link;
        }
    }

    return pendingRemoval && graph.unlink(pendingRemoval->fromNode, pendingRemoval->fromSocket, pendingRemoval->toNode, pendingRemoval->toSocket);
}

void drawNodeBody(SdfGraph& graph, const GraphNodeLayout& layout, const CanvasFrame& frame)
{
    SdfGraphNode& node = *layout.node;
    const bool selected = graph.isNodeSelected(layout.id);
    const bool output = graph.outputNode() == layout.id;
    const ImU32 bodyColor = selected ? IM_COL32(58, 66, 84, 255) : IM_COL32(42, 45, 52, 255);
    const ImU32 titleColor = node.payload.type == SdfNodeType::Output ? IM_COL32(96, 74, 48, 255) : (output ? IM_COL32(76, 96, 70, 255) : IM_COL32(54, 58, 68, 255));
    const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};

    frame.drawList->AddRectFilled(layout.position, nodeEnd, bodyColor, scaleValue(frame, 6.0f));
    frame.drawList->AddRectFilled(layout.position, {nodeEnd.x, layout.position.y + scaleValue(frame, TITLE_HEIGHT)}, titleColor, scaleValue(frame, 6.0f), ImDrawFlags_RoundCornersTop);
    frame.drawList->AddRect(layout.position, nodeEnd, selected ? IM_COL32(120, 170, 255, 255) : IM_COL32(78, 82, 92, 255), scaleValue(frame, 6.0f), 0, scaleValue(frame, selected ? 2.0f : 1.0f));

    const std::string title = graphNodeDisplayName(&node) + (output ? "  [Output]" : "");
    addScaledText(frame, {layout.position.x + scaleValue(frame, 10.0f), layout.position.y + scaleValue(frame, 7.0f)}, IM_COL32(235, 238, 242, 255), title.c_str());
}

bool handleNodeTitleDrag(SdfGraph& graph, const GraphNodeLayout& layout, SdfGraphNodeId& activeDraggedNode)
{
    static SdfGraphNodeId draggedNode = 0;
    bool releasedDraggedNode = false;
    ImGui::SetCursorScreenPos(layout.position);
    const float zoom = layoutZoom(layout);
    const float buttonExtent = std::max(16.0f, 18.0f * zoom);
    const float titleActionWidth = buttonExtent * 2.0f + 14.0f * zoom;
    const float titleDragWidth = std::max(1.0f, layout.size.x - titleActionWidth);
    ImGui::InvisibleButton(("node-title##" + std::to_string(layout.id)).c_str(), {titleDragWidth, TITLE_HEIGHT * zoom});
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        if (ImGui::GetIO().KeyShift) {
            graph.toggleSelectedNode(layout.id);
        } else if (!graph.isNodeSelected(layout.id)) {
            graph.setSelectedNode(layout.id);
        }
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (graph.isNodeSelected(layout.id)) {
            for (const SdfGraphNodeId selectedNode : graph.selectedNodes()) {
                if (SdfGraphNode* node = graph.node(selectedNode)) {
                    node->editorX += delta.x / zoom;
                    node->editorY += delta.y / zoom;
                }
            }
        } else {
            layout.node->editorX += delta.x / zoom;
            layout.node->editorY += delta.y / zoom;
        }
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
    std::string& inputDragCandidateSocket,
    bool& draggingInputLink,
    SdfGraphNodeId& dragInputNode,
    std::string& dragInputSocket,
    bool& dragOutputFromInputDetach)
{
    bool sceneDirty = false;
    SdfGraphNode& node = *layout.node;

    for (size_t i = 0; i < node.inputs.size(); ++i) {
        const SdfGraphSocket& socket = node.inputs[i];
        const ImVec2 pin = {layout.position.x, layout.position.y + scaleValue(frame, TITLE_HEIGHT + 18.0f + static_cast<float>(i) * SOCKET_ROW_HEIGHT)};
        const float pinHitRadius = scaleValue(frame, 12.0f);
        const bool dragHover = draggingLink
            && distanceSquared(pin, ImGui::GetIO().MousePos) <= pinHitRadius * pinHitRadius
            && socketsCompatible(graph, dragOutputNode, dragOutputSocket, layout.id, socket.name);

        frame.drawList->AddCircleFilled(pin, scaleValue(frame, dragHover ? 7.0f : 5.0f), dragHover ? IM_COL32(255, 210, 110, 255) : IM_COL32(120, 180, 120, 255));
        addScaledText(frame, {pin.x + scaleValue(frame, 10.0f), pin.y - scaleValue(frame, 7.0f)}, IM_COL32(220, 224, 230, 255), socketDisplayName(socket).c_str());

        ImGui::SetCursorScreenPos({pin.x - scaleValue(frame, 8.0f), pin.y - scaleValue(frame, 8.0f)});
        ImGui::InvisibleButton(("input##" + std::to_string(layout.id) + "-" + socket.name).c_str(), {scaleValue(frame, 16.0f), scaleValue(frame, 16.0f)});
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
                    dragOutputFromInputDetach = true;
                    sceneDirty = true;
                }
            } else {
                // AGENT: Dragging from an empty input creates a reverse link
                // request; dragging an occupied input only detaches/reconnects.
                draggingInputLink = true;
                dragInputNode = layout.id;
                dragInputSocket = socket.name;
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && inputDragCandidateNode == layout.id && inputDragCandidateSocket == socket.name) {
            inputDragCandidateNode = 0;
            inputDragCandidateSocket.clear();
        }
    }

    return sceneDirty;
}

void drawOutputPins(SdfGraph& graph, const GraphNodeLayout& layout, const CanvasFrame& frame, bool& draggingLink, SdfGraphNodeId& dragOutputNode, std::string& dragOutputSocket, bool& dragOutputFromInputDetach)
{
    SdfGraphNode& node = *layout.node;

    for (size_t i = 0; i < node.outputs.size(); ++i) {
        const SdfGraphSocket& socket = node.outputs[i];
        const ImVec2 pin = {layout.position.x + layout.size.x, layout.position.y + scaleValue(frame, TITLE_HEIGHT + 18.0f + static_cast<float>(i) * SOCKET_ROW_HEIGHT)};
        const bool activeDrag = draggingLink && dragOutputNode == layout.id && dragOutputSocket == socket.name;
        frame.drawList->AddCircleFilled(pin, scaleValue(frame, activeDrag ? 7.0f : 5.0f), activeDrag ? IM_COL32(255, 210, 110, 255) : IM_COL32(120, 160, 240, 255));

        const std::string label = socketDisplayName(socket);
        const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        addScaledText(frame, {pin.x - scaleValue(frame, 10.0f) - scaleValue(frame, labelSize.x), pin.y - scaleValue(frame, 7.0f)}, IM_COL32(220, 224, 230, 255), label.c_str());

        ImGui::SetCursorScreenPos({pin.x - scaleValue(frame, 8.0f), pin.y - scaleValue(frame, 8.0f)});
        ImGui::InvisibleButton(("output##" + std::to_string(layout.id) + "-" + socket.name).c_str(), {scaleValue(frame, 16.0f), scaleValue(frame, 16.0f)});
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            draggingLink = true;
            dragOutputNode = layout.id;
            dragOutputSocket = socket.name;
            dragOutputFromInputDetach = false;
            graph.setSelectedNode(layout.id);
        }
    }
}

bool updateActiveLinkDrag(SdfGraph& graph, const CanvasFrame& frame, const std::vector<GraphSocketAnchor>& anchors, bool& draggingLink, SdfGraphNodeId& dragOutputNode, std::string& dragOutputSocket, bool& releasedOnEmpty)
{
    releasedOnEmpty = false;
    if (!draggingLink) {
        return false;
    }

    if (const std::optional<ImVec2> from = findSocketAnchor(anchors, dragOutputNode, dragOutputSocket, true)) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float handle = scaleValue(frame, 70.0f);
        frame.drawList->AddBezierCubic(*from, {from->x + handle, from->y}, {mouse.x - handle, mouse.y}, mouse, IM_COL32(255, 210, 110, 255), scaleValue(frame, 3.0f));
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        return false;
    }

    bool sceneDirty = false;
    bool linkedTarget = false;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float pinHitRadius = scaleValue(frame, 12.0f);
    for (const GraphSocketAnchor& anchor : anchors) {
        if (anchor.output || distanceSquared(anchor.position, mouse) > pinHitRadius * pinHitRadius) {
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
            linkedTarget = true;
            break;
        }
    }

    releasedOnEmpty = !linkedTarget && ImGui::IsWindowHovered();
    draggingLink = false;
    dragOutputNode = 0;
    dragOutputSocket.clear();
    return sceneDirty;
}

} // namespace sdf3d::node_editor
