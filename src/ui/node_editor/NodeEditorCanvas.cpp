#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/systems/GraphSystem.h"
#include "sdf3d/ui/node_editor/NodeEditorProperties.h"

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
    if (socket.multiInput) {
        label += "+";
    }
    label += " : ";
    label += socketTypeName(socket.type);
    return label;
}

std::string graphNodeDisplayName(const SdfGraphNode* node, const GraphGroupRegistry& groups)
{
    if (node == nullptr) {
        return "None";
    }

    const std::string displayName = GraphSystem::displayNameForNode(*node, groups);
    std::string label = displayName.empty() ? graphNodeTypeName(node->payload.type) : displayName;
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
    const bool materialNode = isSdfMaterialNode(node.payload.type);
    const size_t materialRows = materialNode ? (node.payload.type == SdfNodeType::CheckerMaterial ? 6 : 4) : 0;
    return nameRows + materialRows + visibleInlinePropertyParameterCount(node.payload);
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

std::size_t incomingLinkCount(const SdfGraph& graph, SdfGraphNodeId node, const std::string& socket)
{
    std::size_t count = 0;
    for (const SdfGraphLink& link : graph.links()) {
        if (link.toNode == node && link.toSocket == socket) {
            ++count;
        }
    }
    return count;
}

float multiInputPillHeight(std::size_t linkCount, bool hovered)
{
    const std::size_t visibleCount = std::max<std::size_t>(1, linkCount);
    return (hovered ? 34.0f : 28.0f) + static_cast<float>(visibleCount - 1) * 8.0f;
}

float multiInputRowHeight(const SdfGraph& graph, const SdfGraphNode& node, const SdfGraphSocket& socket)
{
    if (!socket.multiInput) {
        return SOCKET_ROW_HEIGHT;
    }
    return std::max(SOCKET_ROW_HEIGHT, multiInputPillHeight(incomingLinkCount(graph, node.id, socket.name), false) + 6.0f);
}

float inputPinYOffset(const SdfGraph& graph, const SdfGraphNode& node, std::size_t inputIndex)
{
    float offset = TITLE_HEIGHT + 18.0f;
    for (std::size_t i = 0; i < inputIndex; ++i) {
        offset += multiInputRowHeight(graph, node, node.inputs[i]);
    }
    return offset;
}

float inputSocketAreaHeight(const SdfGraph& graph, const SdfGraphNode& node)
{
    float height = 0.0f;
    for (const SdfGraphSocket& input : node.inputs) {
        height += multiInputRowHeight(graph, node, input);
    }
    return height;
}

std::size_t multiInputLinkIndex(const SdfGraph& graph, const SdfGraphLink& targetLink)
{
    std::size_t index = 0;
    for (const SdfGraphLink& link : graph.links()) {
        if (link.toNode != targetLink.toNode || link.toSocket != targetLink.toSocket) {
            continue;
        }
        if (link.fromNode == targetLink.fromNode && link.fromSocket == targetLink.fromSocket) {
            return index;
        }
        ++index;
    }
    return index;
}

std::optional<SdfGraphLink> nearestLinkToMultiInput(
    const SdfGraph& graph,
    SdfGraphNodeId node,
    const std::string& socket,
    float mouseY,
    float pinY,
    float zoom)
{
    std::optional<SdfGraphLink> nearest;
    float nearestDistance = 0.0f;
    const std::size_t count = incomingLinkCount(graph, node, socket);
    std::size_t index = 0;
    for (const SdfGraphLink& link : graph.links()) {
        if (link.toNode != node || link.toSocket != socket) {
            continue;
        }

        const float anchorY = pinY + (static_cast<float>(index) - (static_cast<float>(count) - 1.0f) * 0.5f) * 8.0f * zoom;
        const float distance = std::abs(mouseY - anchorY);
        if (!nearest || distance < nearestDistance) {
            nearest = link;
            nearestDistance = distance;
        }
        ++index;
    }

    return nearest;
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

        const float inputHeight = inputSocketAreaHeight(graph, *node);
        const float outputHeight = static_cast<float>(node->outputs.size()) * SOCKET_ROW_HEIGHT;
        const float socketAreaHeight = std::max({SOCKET_ROW_HEIGHT, inputHeight, outputHeight});
        const float nodeHeight = TITLE_HEIGHT + 34.0f + socketAreaHeight + static_cast<float>(inlinePropertyRows(*node)) * 24.0f;
        const ImVec2 nodePosition = graphToScreen(frame, {24.0f + node->editorX, 24.0f + node->editorY});
        const ImVec2 contentPosition = {nodePosition.x + scaleValue(frame, 10.0f), nodePosition.y + scaleValue(frame, TITLE_HEIGHT + 24.0f + socketAreaHeight)};
        layouts.push_back({ids[index], node, nodePosition, {scaleValue(frame, NODE_WIDTH), scaleValue(frame, nodeHeight)}, contentPosition});

        for (size_t i = 0; i < node->inputs.size(); ++i) {
            anchors.push_back({ids[index], node->inputs[i].name, false, {nodePosition.x, nodePosition.y + scaleValue(frame, inputPinYOffset(graph, *node, i))}});
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
        std::optional<ImVec2> to = findSocketAnchor(anchors, link.toNode, link.toSocket, false);
        if (!from || !to) {
            continue;
        }
        if (const SdfGraphNode* target = graph.node(link.toNode)) {
            if (const SdfGraphSocket* socket = findSocket(target->inputs, link.toSocket, SdfSocketDirection::Input);
                socket != nullptr && socket->multiInput) {
                const std::size_t count = incomingLinkCount(graph, link.toNode, link.toSocket);
                const std::size_t index = multiInputLinkIndex(graph, link);
                const float offset = (static_cast<float>(index) - (static_cast<float>(count) - 1.0f) * 0.5f) * scaleValue(frame, 8.0f);
                to->y += offset;
            }
        }

        const float handle = scaleValue(frame, 70.0f);
        frame.drawList->AddBezierCubic(*from, {from->x + handle, from->y}, {to->x - handle, to->y}, *to, IM_COL32(130, 170, 255, 255), scaleValue(frame, 3.0f));
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && mouseNearBezier(ImGui::GetIO().MousePos, *from, *to)) {
            pendingRemoval = link;
        }
    }

    return pendingRemoval && graph.unlink(pendingRemoval->fromNode, pendingRemoval->fromSocket, pendingRemoval->toNode, pendingRemoval->toSocket);
}

void drawEmptyGraphMessage(const CanvasFrame& frame, const std::vector<GraphNodeLayout>& layouts)
{
    if (layouts.empty()) {
        frame.drawList->AddText({frame.origin.x + 16.0f, frame.origin.y + 16.0f}, IM_COL32(210, 215, 225, 255), "Empty graph");
    }
}

bool mouseInsideAnyNode(const std::vector<GraphNodeLayout>& layouts, ImVec2 mouse)
{
    for (const GraphNodeLayout& layout : layouts) {
        const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};
        if (mouse.x >= layout.position.x && mouse.x <= nodeEnd.x && mouse.y >= layout.position.y && mouse.y <= nodeEnd.y) {
            return true;
        }
    }
    return false;
}

NodeDrawResult drawGraphNodes(
    SdfGraph& graph,
    GraphGroupRegistry& groups,
    const CanvasFrame& frame,
    const std::vector<GraphNodeLayout>& layouts,
    const std::vector<GraphSocketAnchor>& anchors,
    NodeEditorDragState& drag,
    std::vector<SdfGraphNodeId>& pendingDelete)
{
    NodeDrawResult result;
    for (const GraphNodeLayout& layout : layouts) {
        drawNodeBody(graph, groups, layout, frame);
        drawInactiveNodePreview(graph, layout, frame, anchors);
        if (handleNodeTitleDrag(graph, layout, result.activeDraggedNode, result.requestedGroupEnterNode)) {
            result.releasedDraggedNode = layout.id;
        }
        if (drawInputPins(
                graph,
                layout,
                frame,
                drag.draggingLink,
                drag.dragOutputNode,
                drag.dragOutputSocket,
                drag.inputDragCandidateNode,
                drag.inputDragCandidateSocket,
                drag.draggingInputLink,
                drag.dragInputNode,
                drag.dragInputSocket,
                drag.dragOutputFromInputDetach,
                drag.detachedInputNode,
                drag.detachedInputSocket)) {
            result.dirty.scene = true;
        }
        drawOutputPins(graph, layout, frame, drag.draggingLink, drag.dragOutputNode, drag.dragOutputSocket, drag.dragOutputFromInputDetach);
        SdfGraphNodeId actionDelete = 0;
        if (drawNodeActions(graph, layout, actionDelete)) {
            result.dirty.scene = true;
        }
        if (actionDelete != 0) {
            if (graph.isNodeSelected(actionDelete)) {
                for (const SdfGraphNodeId id : graph.selectedNodes()) {
                    if (!graph.isOutputNode(id) && graph.node(id) != nullptr) {
                        pendingDelete.push_back(id);
                    }
                }
            } else {
                pendingDelete.push_back(actionDelete);
            }
        }
        const EditorDirtyState inlineDirty = drawNodeInlineProperties(graph, groups, layout, frame);
        result.dirty.scene = result.dirty.scene || inlineDirty.scene;
        result.dirty.material = result.dirty.material || inlineDirty.material;
    }
    return result;
}

bool drawNodeDragInsertion(
    SdfGraph& graph,
    const CanvasFrame& frame,
    const std::vector<GraphNodeLayout>& layouts,
    const std::vector<GraphSocketAnchor>& anchors,
    SdfGraphNodeId activeDraggedNode,
    SdfGraphNodeId releasedDraggedNode)
{
    if (activeDraggedNode != 0) {
        for (const GraphNodeLayout& layout : layouts) {
            if (layout.id == activeDraggedNode) {
                drawLinkInsertionPreview(graph, layout, frame, anchors);
                break;
            }
        }
    }

    if (releasedDraggedNode == 0) {
        return false;
    }
    for (const GraphNodeLayout& layout : layouts) {
        if (layout.id == releasedDraggedNode && insertNodeIntoLink(graph, layout, anchors)) {
            return true;
        }
    }
    return false;
}

void openNodeEditorContextPopup(
    const CanvasFrame& frame,
    bool removedLink,
    const NodeEditorDragState& drag,
    const SelectionRectState& selection,
    bool mouseInsideNode,
    const LinkDragResult& linkDrag,
    NodeEditorPopupState& popup)
{
    if (!removedLink
        && !drag.draggingLink
        && !drag.draggingInputLink
        && !selection.dragging
        && !mouseInsideNode
        && ImGui::IsWindowHovered()
        && ImGui::IsMouseClicked(ImGuiMouseButton_Right)
        && !ImGui::IsAnyItemHovered()) {
        const ImVec2 popupPosition = canvasMouseGraphPosition(frame);
        popup.editorX = popupPosition.x;
        popup.editorY = popupPosition.y;
        popup.linkFromNode = 0;
        popup.linkFromSocket.clear();
        popup.linkToNode = 0;
        popup.linkToSocket.clear();
        ImGui::OpenPopup(NODE_ADD_POPUP_ID);
    }
    if (linkDrag.releasedLinkOnEmpty || linkDrag.releasedInputLinkOnEmpty) {
        const ImVec2 popupPosition = canvasMouseGraphPosition(frame);
        popup.editorX = popupPosition.x;
        popup.editorY = popupPosition.y;
        if (linkDrag.releasedInputLinkOnEmpty && linkDrag.releasedToNode != 0 && !linkDrag.releasedToSocket.empty()) {
            popup.linkFromNode = linkDrag.releasedFromNode;
            popup.linkFromSocket = linkDrag.releasedFromSocket;
            popup.linkToNode = linkDrag.releasedToNode;
            popup.linkToSocket = linkDrag.releasedToSocket;
        } else {
            popup.linkFromNode = linkDrag.releasedFromNode;
            popup.linkFromSocket = linkDrag.releasedFromSocket;
            popup.linkToNode = 0;
            popup.linkToSocket.clear();
        }
        ImGui::OpenPopup(NODE_ADD_POPUP_ID);
    }
}

void drawNodeBody(SdfGraph& graph, GraphGroupRegistry& groups, const GraphNodeLayout& layout, const CanvasFrame& frame)
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

    const std::string title = graphNodeDisplayName(&node, groups) + (output ? "  [Output]" : "");
    addScaledText(frame, {layout.position.x + scaleValue(frame, 10.0f), layout.position.y + scaleValue(frame, 7.0f)}, IM_COL32(235, 238, 242, 255), title.c_str());
}

bool handleNodeTitleDrag(SdfGraph& graph, const GraphNodeLayout& layout, SdfGraphNodeId& activeDraggedNode, SdfGraphNodeId& requestedGroupEnterNode)
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
        if (layout.node->payload.type == SdfNodeType::Group && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            requestedGroupEnterNode = layout.id;
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
    bool& dragOutputFromInputDetach,
    SdfGraphNodeId& detachedInputNode,
    std::string& detachedInputSocket)
{
    bool sceneDirty = false;
    SdfGraphNode& node = *layout.node;

    for (size_t i = 0; i < node.inputs.size(); ++i) {
        const SdfGraphSocket& socket = node.inputs[i];
        const ImVec2 pin = {layout.position.x, layout.position.y + scaleValue(frame, inputPinYOffset(graph, node, i))};
        const float pinHitRadius = scaleValue(frame, 12.0f);
        const bool dragHover = draggingLink
            && distanceSquared(pin, ImGui::GetIO().MousePos) <= pinHitRadius * pinHitRadius
            && socketsCompatible(graph, dragOutputNode, dragOutputSocket, layout.id, socket.name);

        const ImU32 pinColor = dragHover ? IM_COL32(255, 210, 110, 255) : IM_COL32(120, 180, 120, 255);
        if (socket.multiInput) {
            const float pillWidth = scaleValue(frame, dragHover ? 10.0f : 8.0f);
            const float pillHeight = scaleValue(frame, multiInputPillHeight(incomingLinkCount(graph, layout.id, socket.name), dragHover));
            frame.drawList->AddRectFilled(
                {pin.x - pillWidth * 0.5f, pin.y - pillHeight * 0.5f},
                {pin.x + pillWidth * 0.5f, pin.y + pillHeight * 0.5f},
                pinColor,
                pillHeight * 0.5f);
        } else {
            frame.drawList->AddCircleFilled(pin, scaleValue(frame, dragHover ? 7.0f : 5.0f), pinColor);
        }
        addScaledText(frame, {pin.x + scaleValue(frame, 10.0f), pin.y - scaleValue(frame, 7.0f)}, IM_COL32(220, 224, 230, 255), socketDisplayName(socket).c_str());

        const float inputHitWidth = scaleValue(frame, socket.multiInput ? 18.0f : 16.0f);
        const float inputHitHeight = socket.multiInput
            ? scaleValue(frame, multiInputPillHeight(incomingLinkCount(graph, layout.id, socket.name), dragHover) + 8.0f)
            : scaleValue(frame, 16.0f);
        ImGui::SetCursorScreenPos({pin.x - inputHitWidth * 0.5f, pin.y - inputHitHeight * 0.5f});
        ImGui::InvisibleButton(("input##" + std::to_string(layout.id) + "-" + socket.name).c_str(), {inputHitWidth, inputHitHeight});
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
            const std::optional<SdfGraphLink> existing = socket.multiInput
                ? nearestLinkToMultiInput(graph, layout.id, socket.name, ImGui::GetIO().MousePos.y, pin.y, frame.zoom)
                : linkToInput(graph, layout.id, socket.name);
            if (existing) {
                if (graph.unlink(existing->fromNode, existing->fromSocket, existing->toNode, existing->toSocket)) {
                    draggingLink = true;
                    dragOutputNode = existing->fromNode;
                    dragOutputSocket = existing->fromSocket;
                    dragOutputFromInputDetach = true;
                    detachedInputNode = layout.id;
                    detachedInputSocket = socket.name;
                    sceneDirty = true;
                }
            } else {
                // AGENT: Dragging from an empty input creates a reverse link
                // request; multi-input sockets keep existing wires until a wire is removed directly.
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
