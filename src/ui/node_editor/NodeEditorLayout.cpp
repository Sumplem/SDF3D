#include "sdf3d/ui/node_editor/NodeEditorLayout.h"

#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"
#include "sdf3d/ui/node_editor/NodeEditorProperties.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sdf3d::node_editor {
namespace {

constexpr float horizontalGap = 140.0f;
constexpr float verticalGap = 24.0f;
constexpr float positionEpsilon = 0.001f;
constexpr float graphPositionOffset = 24.0f;

struct LayoutEdges {
    std::unordered_map<SdfGraphNodeId, std::vector<SdfGraphLink>> outgoing;
    std::unordered_map<SdfGraphNodeId, std::vector<SdfGraphLink>> incoming;
    std::unordered_map<SdfGraphNodeId, int> indegree;
};

std::vector<SdfGraphNodeId> sortedLayoutNodeIds(const SdfGraph& graph, bool selectedOnly)
{
    std::vector<SdfGraphNodeId> ids;
    if (selectedOnly) {
        for (const SdfGraphNodeId id : graph.selectedNodes()) {
            if (graph.node(id) != nullptr) {
                ids.push_back(id);
            }
        }
    } else {
        ids.reserve(graph.nodes().size());
        for (const auto& [id, node] : graph.nodes()) {
            (void)node;
            ids.push_back(id);
        }
    }

    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

std::unordered_set<SdfGraphNodeId> nodeSetFor(const std::vector<SdfGraphNodeId>& ids)
{
    std::unordered_set<SdfGraphNodeId> set;
    set.reserve(ids.size());
    for (const SdfGraphNodeId id : ids) {
        set.insert(id);
    }
    return set;
}

int inputSocketOrder(const SdfGraph& graph, SdfGraphNodeId nodeId, const std::string& socket)
{
    const SdfGraphNode* node = graph.node(nodeId);
    if (node == nullptr) {
        return 1000;
    }

    if (const SdfNodeDefinition* definition = sdfNodeDefinition(node->payload.type)) {
        for (size_t i = 0; i < definition->inputs.size(); ++i) {
            if (definition->inputs[i].name == socket) {
                return static_cast<int>(i);
            }
        }
    }

    for (size_t i = 0; i < node->inputs.size(); ++i) {
        if (node->inputs[i].name == socket) {
            return static_cast<int>(i);
        }
    }
    return 1000;
}

bool outgoingLinkLess(const SdfGraph& graph, const SdfGraphLink& left, const SdfGraphLink& right)
{
    const int leftSocketOrder = inputSocketOrder(graph, left.toNode, left.toSocket);
    const int rightSocketOrder = inputSocketOrder(graph, right.toNode, right.toSocket);
    if (leftSocketOrder != rightSocketOrder) {
        return leftSocketOrder < rightSocketOrder;
    }
    if (left.toNode != right.toNode) {
        return left.toNode < right.toNode;
    }
    return left.toSocket < right.toSocket;
}

LayoutEdges buildIncludedEdges(const SdfGraph& graph, const std::vector<SdfGraphNodeId>& ids)
{
    LayoutEdges edges;
    const std::unordered_set<SdfGraphNodeId> layoutNodes = nodeSetFor(ids);
    for (const SdfGraphNodeId id : ids) {
        edges.indegree[id] = 0;
    }

    for (const SdfGraphLink& link : graph.links()) {
        if (layoutNodes.find(link.fromNode) == layoutNodes.end() || layoutNodes.find(link.toNode) == layoutNodes.end()) {
            continue;
        }
        edges.outgoing[link.fromNode].push_back(link);
        edges.incoming[link.toNode].push_back(link);
        ++edges.indegree[link.toNode];
    }

    for (auto& [node, links] : edges.outgoing) {
        (void)node;
        std::sort(links.begin(), links.end(), [&](const SdfGraphLink& left, const SdfGraphLink& right) {
            return outgoingLinkLess(graph, left, right);
        });
    }
    for (auto& [node, links] : edges.incoming) {
        (void)node;
        std::sort(links.begin(), links.end(), [](const SdfGraphLink& left, const SdfGraphLink& right) {
            if (left.fromNode != right.fromNode) {
                return left.fromNode < right.fromNode;
            }
            return left.toSocket < right.toSocket;
        });
    }

    return edges;
}

std::vector<SdfGraphNodeId> topologicalOrder(
    const std::vector<SdfGraphNodeId>& ids,
    const LayoutEdges& edges,
    std::unordered_map<SdfGraphNodeId, int>& columnByNode)
{
    std::unordered_map<SdfGraphNodeId, int> indegree = edges.indegree;

    for (const SdfGraphNodeId id : ids) {
        columnByNode[id] = 0;
    }

    std::priority_queue<SdfGraphNodeId, std::vector<SdfGraphNodeId>, std::greater<SdfGraphNodeId>> ready;
    for (const SdfGraphNodeId id : ids) {
        if (indegree[id] == 0) {
            ready.push(id);
        }
    }

    std::unordered_set<SdfGraphNodeId> processed;
    std::vector<SdfGraphNodeId> order;
    order.reserve(ids.size());
    while (!ready.empty()) {
        const SdfGraphNodeId id = ready.top();
        ready.pop();
        processed.insert(id);
        order.push_back(id);

        const auto outgoing = edges.outgoing.find(id);
        if (outgoing == edges.outgoing.end()) {
            continue;
        }
        for (const SdfGraphLink& link : outgoing->second) {
            columnByNode[link.toNode] = std::max(columnByNode[link.toNode], columnByNode[id] + 1);
            --indegree[link.toNode];
            if (indegree[link.toNode] == 0) {
                ready.push(link.toNode);
            }
        }
    }

    int fallbackColumn = 0;
    for (const auto& [id, column] : columnByNode) {
        (void)id;
        fallbackColumn = std::max(fallbackColumn, column + 1);
    }
    for (const SdfGraphNodeId id : ids) {
        if (processed.find(id) == processed.end()) {
            columnByNode[id] = fallbackColumn++;
            order.push_back(id);
        }
    }

    return order;
}

std::vector<SdfGraphNodeId> sinkNodes(const std::vector<SdfGraphNodeId>& ids, const LayoutEdges& edges)
{
    std::vector<SdfGraphNodeId> sinks;
    for (const SdfGraphNodeId id : ids) {
        const auto outgoing = edges.outgoing.find(id);
        if (outgoing == edges.outgoing.end() || outgoing->second.empty()) {
            sinks.push_back(id);
        }
    }
    return sinks;
}

void propagateReverseDepths(
    const LayoutEdges& edges,
    const std::vector<SdfGraphNodeId>& anchors,
    std::unordered_map<SdfGraphNodeId, int>& reverseDepthByNode)
{
    std::queue<SdfGraphNodeId> pending;
    for (const SdfGraphNodeId anchor : anchors) {
        const auto [it, inserted] = reverseDepthByNode.emplace(anchor, 0);
        if (!inserted && it->second != 0) {
            it->second = 0;
        }
        pending.push(anchor);
    }

    while (!pending.empty()) {
        const SdfGraphNodeId id = pending.front();
        pending.pop();
        const int depth = reverseDepthByNode[id];
        const auto incoming = edges.incoming.find(id);
        if (incoming == edges.incoming.end()) {
            continue;
        }

        for (const SdfGraphLink& link : incoming->second) {
            const int parentDepth = depth + 1;
            const auto it = reverseDepthByNode.find(link.fromNode);
            if (it == reverseDepthByNode.end() || parentDepth > it->second) {
                reverseDepthByNode[link.fromNode] = parentDepth;
                pending.push(link.fromNode);
            }
        }
    }
}

void assignColumnsFromOutput(
    const SdfGraph& graph,
    const std::vector<SdfGraphNodeId>& ids,
    const LayoutEdges& edges,
    std::unordered_map<SdfGraphNodeId, int>& columnByNode)
{
    std::unordered_map<SdfGraphNodeId, int> reverseDepthByNode;
    const SdfGraphNodeId output = graph.outputNode();
    if (std::find(ids.begin(), ids.end(), output) != ids.end()) {
        propagateReverseDepths(edges, {output}, reverseDepthByNode);
    }

    std::vector<SdfGraphNodeId> fallbackSinks;
    for (const SdfGraphNodeId sink : sinkNodes(ids, edges)) {
        if (reverseDepthByNode.find(sink) == reverseDepthByNode.end()) {
            fallbackSinks.push_back(sink);
        }
    }
    propagateReverseDepths(edges, fallbackSinks, reverseDepthByNode);

    int fallbackDepth = 0;
    for (const auto& [node, depth] : reverseDepthByNode) {
        (void)node;
        fallbackDepth = std::max(fallbackDepth, depth + 1);
    }
    for (const SdfGraphNodeId id : ids) {
        if (reverseDepthByNode.find(id) == reverseDepthByNode.end()) {
            reverseDepthByNode[id] = fallbackDepth++;
        }
    }

    int maxDepth = 0;
    for (const auto& [node, depth] : reverseDepthByNode) {
        (void)node;
        maxDepth = std::max(maxDepth, depth);
    }
    for (const SdfGraphNodeId id : ids) {
        columnByNode[id] = maxDepth - reverseDepthByNode[id];
    }
}

std::map<int, std::vector<SdfGraphNodeId>> columnsFor(
    const std::vector<SdfGraphNodeId>& topologicalIds,
    const std::unordered_map<SdfGraphNodeId, int>& columnByNode)
{
    std::map<int, std::vector<SdfGraphNodeId>> columns;
    for (const SdfGraphNodeId id : topologicalIds) {
        columns[columnByNode.at(id)].push_back(id);
    }
    return columns;
}

size_t nextFreeRow(const std::unordered_set<size_t>& occupiedRows, size_t startRow)
{
    size_t row = startRow;
    while (occupiedRows.find(row) != occupiedRows.end()) {
        ++row;
    }
    return row;
}

void placeNodeInColumn(
    SdfGraphNodeId id,
    size_t preferredRow,
    std::unordered_map<SdfGraphNodeId, size_t>& rowByNode,
    std::unordered_set<size_t>& occupiedRows)
{
    if (rowByNode.find(id) != rowByNode.end()) {
        return;
    }

    const size_t row = nextFreeRow(occupiedRows, preferredRow);
    rowByNode[id] = row;
    occupiedRows.insert(row);
}

void assignRows(
    const std::map<int, std::vector<SdfGraphNodeId>>& columns,
    const std::unordered_map<SdfGraphNodeId, int>& columnByNode,
    const LayoutEdges& edges,
    std::unordered_map<SdfGraphNodeId, size_t>& rowByNode,
    std::map<int, std::unordered_set<size_t>>& occupiedRowsByColumn)
{
    for (const auto& [columnIndex, column] : columns) {
        std::unordered_set<size_t>& occupiedRows = occupiedRowsByColumn[columnIndex];
        for (const SdfGraphNodeId id : column) {
            placeNodeInColumn(id, 0, rowByNode, occupiedRows);
        }

        for (const SdfGraphNodeId id : column) {
            const auto sourceRow = rowByNode.find(id);
            if (sourceRow == rowByNode.end()) {
                continue;
            }

            const auto outgoing = edges.outgoing.find(id);
            if (outgoing == edges.outgoing.end()) {
                continue;
            }

            size_t childIndex = 0;
            for (const SdfGraphLink& link : outgoing->second) {
                const auto targetColumn = columnByNode.find(link.toNode);
                if (targetColumn == columnByNode.end() || targetColumn->second <= columnIndex) {
                    continue;
                }

                const size_t preferredRow = sourceRow->second + childIndex;
                placeNodeInColumn(link.toNode, preferredRow, rowByNode, occupiedRowsByColumn[targetColumn->second]);
                ++childIndex;
            }
        }
    }
}

size_t maxOccupiedRows(const std::map<int, std::unordered_set<size_t>>& occupiedRowsByColumn)
{
    size_t maxRows = 0;
    for (const auto& [column, occupiedRows] : occupiedRowsByColumn) {
        (void)column;
        for (const size_t row : occupiedRows) {
            maxRows = std::max(maxRows, row + 1);
        }
    }
    return maxRows;
}

ImVec2 visibleCanvasCenterGraph(const CanvasFrame& frame)
{
    const ImVec2 screenCenter = {(frame.origin.x + frame.end.x) * 0.5f, (frame.origin.y + frame.end.y) * 0.5f};
    ImVec2 graphCenter = {
        (screenCenter.x - frame.origin.x - frame.pan.x) / frame.zoom,
        (screenCenter.y - frame.origin.y - frame.pan.y) / frame.zoom,
    };
    graphCenter.x -= graphPositionOffset;
    graphCenter.y -= graphPositionOffset;
    return graphCenter;
}

bool setNodePosition(SdfGraphNode& node, float x, float y)
{
    if (std::abs(node.editorX - x) <= positionEpsilon && std::abs(node.editorY - y) <= positionEpsilon) {
        return false;
    }

    node.editorX = x;
    node.editorY = y;
    return true;
}

size_t inlinePropertyRows(const SdfGraphNode& node)
{
    if (node.editorPropertiesCollapsed) {
        return 0;
    }

    constexpr size_t nameRows = 1;
    const size_t materialOverrideRows = node.payload.type == SdfNodeType::MaterialOverride ? 1 : 0;
    return nameRows + materialOverrideRows + visibleInlinePropertyParameterCount(node.payload);
}

float estimatedNodeHeight(const SdfGraphNode& node)
{
    const size_t socketRows = std::max(node.inputs.size(), node.outputs.size());
    return TITLE_HEIGHT + 34.0f + std::max<size_t>(1, socketRows) * SOCKET_ROW_HEIGHT + static_cast<float>(inlinePropertyRows(node)) * 24.0f;
}

float standardNodeHeight(const SdfGraph& graph, const std::vector<SdfGraphNodeId>& ids)
{
    float height = 0.0f;
    for (const SdfGraphNodeId id : ids) {
        const SdfGraphNode* node = graph.node(id);
        if (node != nullptr) {
            height = std::max(height, estimatedNodeHeight(*node));
        }
    }
    return height;
}

} // namespace

bool autoLayoutGraph(SdfGraph& graph, const CanvasFrame& frame, bool selectedOnly)
{
    const std::vector<SdfGraphNodeId> ids = sortedLayoutNodeIds(graph, selectedOnly);
    if (ids.empty()) {
        return false;
    }

    const LayoutEdges edges = buildIncludedEdges(graph, ids);
    std::unordered_map<SdfGraphNodeId, int> columnByNode;
    const std::vector<SdfGraphNodeId> topologicalIds = topologicalOrder(ids, edges, columnByNode);
    assignColumnsFromOutput(graph, ids, edges, columnByNode);
    const std::map<int, std::vector<SdfGraphNodeId>> columns = columnsFor(topologicalIds, columnByNode);

    std::unordered_map<SdfGraphNodeId, size_t> rowByNode;
    std::map<int, std::unordered_set<size_t>> occupiedRowsByColumn;
    assignRows(columns, columnByNode, edges, rowByNode, occupiedRowsByColumn);

    const ImVec2 center = visibleCanvasCenterGraph(frame);
    const float columnSpacing = NODE_WIDTH + horizontalGap;
    const float totalWidth = static_cast<float>(columns.size() - 1) * columnSpacing;
    const float startX = center.x - totalWidth * 0.5f;
    const float nodeHeight = standardNodeHeight(graph, ids);
    const size_t occupiedRows = std::max<size_t>(1, maxOccupiedRows(occupiedRowsByColumn));
    const float totalHeight = static_cast<float>(occupiedRows) * (nodeHeight + verticalGap);
    const float topY = center.y - totalHeight * 0.5f;

    bool changed = false;
    size_t visualColumn = 0;
    for (const auto& [columnIndex, column] : columns) {
        (void)columnIndex;
        for (const SdfGraphNodeId id : column) {
            SdfGraphNode* node = graph.node(id);
            if (node == nullptr) {
                continue;
            }
            const float x = startX + static_cast<float>(visualColumn) * columnSpacing;
            const float y = topY + static_cast<float>(rowByNode[id]) * (nodeHeight + verticalGap);
            changed = setNodePosition(*node, x, y) || changed;
        }
        ++visualColumn;
    }

    return changed;
}

} // namespace sdf3d::node_editor
