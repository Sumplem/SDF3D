#pragma once

#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/ui/EditorDirtyState.h"

#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

namespace sdf3d::node_editor {

constexpr const char* NODE_ADD_POPUP_ID = "NodeAddPopup";
constexpr float NODE_WIDTH = 220.0f;
constexpr float TITLE_HEIGHT = 28.0f;
constexpr float SOCKET_ROW_HEIGHT = 22.0f;
constexpr float PIN_HIT_RADIUS_SQUARED = 144.0f;

struct GraphNodeLayout {
    SdfGraphNodeId id = 0;
    SdfGraphNode* node = nullptr;
    ImVec2 position = {0.0f, 0.0f};
    ImVec2 size = {0.0f, 0.0f};
    ImVec2 contentPosition = {0.0f, 0.0f};
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
    ImVec2 pan = {0.0f, 0.0f};
    float zoom = 1.0f;
    ImDrawList* drawList = nullptr;
};

std::optional<ImVec2> findSocketAnchor(
    const std::vector<GraphSocketAnchor>& anchors,
    SdfGraphNodeId node,
    const std::string& socket,
    bool output);
const SdfGraphSocket* findSocket(const std::vector<SdfGraphSocket>& sockets, const std::string& name, SdfSocketDirection direction);
bool socketsCompatible(const SdfGraph& graph, SdfGraphNodeId fromNode, const std::string& fromSocket, SdfGraphNodeId toNode, const std::string& toSocket);
bool activeOutputNodeExists(const SdfGraph& graph);
float distanceSquared(ImVec2 a, ImVec2 b);
std::optional<SdfGraphLink> linkToInput(const SdfGraph& graph, SdfGraphNodeId node, const std::string& socket);
bool mouseNearBezier(ImVec2 mouse, ImVec2 from, ImVec2 to);
ImVec2 canvasMouseGraphPosition(const CanvasFrame& frame);

bool previewSelectedNode(SdfGraph& graph);
std::optional<SdfGraphLink> firstLinkFromOutput(const SdfGraph& graph, SdfGraphNodeId node, const std::string& socket);
bool nodeHasMissingRequiredInput(const SdfGraph& graph, const SdfGraphNode& node);
float scaleValue(const CanvasFrame& frame, float value);
ImVec2 graphToScreen(const CanvasFrame& frame, ImVec2 graphPosition);
ImVec2 screenToGraph(const CanvasFrame& frame, ImVec2 screenPosition);
bool insertNodeIntoLink(SdfGraph& graph, const GraphNodeLayout& layout, const std::vector<GraphSocketAnchor>& anchors);
void drawLinkInsertionPreview(const SdfGraph& graph, const GraphNodeLayout& layout, const CanvasFrame& frame, const std::vector<GraphSocketAnchor>& anchors);
void drawInactiveNodePreview(const SdfGraph& graph, const GraphNodeLayout& layout, const CanvasFrame& frame, const std::vector<GraphSocketAnchor>& anchors);

CanvasFrame beginCanvas(float panX, float panY, float zoom);
void updateCanvasView(CanvasFrame& frame, float& panX, float& panY, float& zoom);
void drawGrid(const CanvasFrame& frame);
void buildLayoutsAndAnchors(SdfGraph& graph, const CanvasFrame& frame, std::vector<GraphNodeLayout>& layouts, std::vector<GraphSocketAnchor>& anchors);
bool drawExistingLinks(SdfGraph& graph, const CanvasFrame& frame, const std::vector<GraphSocketAnchor>& anchors);
void drawNodeBody(SdfGraph& graph, const GraphNodeLayout& layout, const CanvasFrame& frame);
bool handleNodeTitleDrag(SdfGraph& graph, const GraphNodeLayout& layout, SdfGraphNodeId& activeDraggedNode);
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
    bool& dragOutputFromInputDetach);
void drawOutputPins(SdfGraph& graph, const GraphNodeLayout& layout, const CanvasFrame& frame, bool& draggingLink, SdfGraphNodeId& dragOutputNode, std::string& dragOutputSocket, bool& dragOutputFromInputDetach);
bool drawNodeActions(SdfGraph& graph, const GraphNodeLayout& layout, SdfGraphNodeId& pendingDelete);
EditorDirtyState drawNodeInlineProperties(const GraphNodeLayout& layout, const CanvasFrame& frame);
bool updateActiveLinkDrag(SdfGraph& graph, const CanvasFrame& frame, const std::vector<GraphSocketAnchor>& anchors, bool& draggingLink, SdfGraphNodeId& dragOutputNode, std::string& dragOutputSocket, bool& releasedOnEmpty);
bool updateActiveInputLinkDrag(SdfGraph& graph, const CanvasFrame& frame, const std::vector<GraphSocketAnchor>& anchors, bool& draggingInputLink, SdfGraphNodeId& dragInputNode, std::string& dragInputSocket, bool& releasedOnEmpty);

} // namespace sdf3d::node_editor
