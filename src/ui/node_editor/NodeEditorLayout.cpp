#include "sdf3d/ui/node_editor/NodeEditorLayout.h"

#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"
#include "sdf3d/ui/node_editor/NodeEditorProperties.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

struct LayoutBlock {
    std::vector<SdfGraphNodeId> nodes;
    std::vector<SdfGraphNodeId> anchors;
    size_t rowCount = 1;
    int maxColumn = 0;
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
        std::stable_sort(links.begin(), links.end(), [&](const SdfGraphLink& left, const SdfGraphLink& right) {
            const int leftSocketOrder = inputSocketOrder(graph, left.toNode, left.toSocket);
            const int rightSocketOrder = inputSocketOrder(graph, right.toNode, right.toSocket);
            if (leftSocketOrder != rightSocketOrder) {
                return leftSocketOrder < rightSocketOrder;
            }
            if (left.toSocket != right.toSocket) {
                return left.toSocket < right.toSocket;
            }
            return false;
        });
    }

    return edges;
}

std::vector<SdfGraphNodeId> sinkNodesInSet(const std::unordered_set<SdfGraphNodeId>& ids, const LayoutEdges& edges)
{
    std::vector<SdfGraphNodeId> sinks;
    for (const SdfGraphNodeId id : ids) {
        bool hasOutgoingInSet = false;
        const auto outgoing = edges.outgoing.find(id);
        if (outgoing != edges.outgoing.end()) {
            for (const SdfGraphLink& link : outgoing->second) {
                if (ids.find(link.toNode) != ids.end()) {
                    hasOutgoingInSet = true;
                    break;
                }
            }
        }
        if (!hasOutgoingInSet) {
            sinks.push_back(id);
        }
    }
    std::sort(sinks.begin(), sinks.end());
    return sinks;
}

std::vector<SdfGraphNodeId> collectIncomingReachable(
    const std::vector<SdfGraphNodeId>& anchors,
    const LayoutEdges& edges,
    const std::unordered_set<SdfGraphNodeId>& allowed)
{
    std::vector<SdfGraphNodeId> result;
    std::unordered_set<SdfGraphNodeId> visited;
    std::queue<SdfGraphNodeId> pending;
    for (const SdfGraphNodeId anchor : anchors) {
        if (allowed.find(anchor) != allowed.end() && visited.insert(anchor).second) {
            pending.push(anchor);
        }
    }

    while (!pending.empty()) {
        const SdfGraphNodeId id = pending.front();
        pending.pop();
        result.push_back(id);

        const auto incoming = edges.incoming.find(id);
        if (incoming == edges.incoming.end()) {
            continue;
        }
        for (const SdfGraphLink& link : incoming->second) {
            if (allowed.find(link.fromNode) != allowed.end() && visited.insert(link.fromNode).second) {
                pending.push(link.fromNode);
            }
        }
    }

    std::sort(result.begin(), result.end());
    return result;
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

int maxColumnForBlock(const std::vector<SdfGraphNodeId>& ids, const std::unordered_map<SdfGraphNodeId, int>& columnByNode)
{
    int maxColumn = 0;
    for (const SdfGraphNodeId id : ids) {
        const auto it = columnByNode.find(id);
        if (it != columnByNode.end()) {
            maxColumn = std::max(maxColumn, it->second);
        }
    }
    return maxColumn;
}

void assignColumnsForBlock(
    const LayoutEdges& edges,
    const LayoutBlock& block,
    std::unordered_map<SdfGraphNodeId, int>& columnByNode)
{
    std::unordered_map<SdfGraphNodeId, int> reverseDepthByNode;
    propagateReverseDepths(edges, block.anchors, reverseDepthByNode);

    int maxDepth = 0;
    for (const SdfGraphNodeId id : block.nodes) {
        const auto it = reverseDepthByNode.find(id);
        if (it != reverseDepthByNode.end()) {
            maxDepth = std::max(maxDepth, it->second);
        }
    }
    for (const SdfGraphNodeId id : block.nodes) {
        const auto it = reverseDepthByNode.find(id);
        columnByNode[id] = it == reverseDepthByNode.end() ? 0 : maxDepth - it->second;
    }
}

size_t assignDfsRows(
    SdfGraphNodeId id,
    size_t baseRow,
    const LayoutEdges& edges,
    const std::unordered_set<SdfGraphNodeId>& blockNodes,
    std::unordered_map<SdfGraphNodeId, size_t>& rowByNode,
    std::unordered_set<SdfGraphNodeId>& visiting)
{
    if (blockNodes.find(id) == blockNodes.end()) {
        return baseRow;
    }

    const auto existing = rowByNode.find(id);
    if (existing != rowByNode.end()) {
        return std::max(baseRow + 1, existing->second + 1);
    }
    if (!visiting.insert(id).second) {
        rowByNode[id] = baseRow;
        return baseRow + 1;
    }

    rowByNode[id] = baseRow;
    size_t nextChildBaseRow = baseRow;
    const auto incoming = edges.incoming.find(id);
    if (incoming != edges.incoming.end()) {
        for (const SdfGraphLink& link : incoming->second) {
            if (blockNodes.find(link.fromNode) == blockNodes.end()) {
                continue;
            }
            const size_t childEndRow = assignDfsRows(link.fromNode, nextChildBaseRow, edges, blockNodes, rowByNode, visiting);
            nextChildBaseRow = std::max(nextChildBaseRow + 1, childEndRow);
        }
    }

    visiting.erase(id);
    return std::max(baseRow + 1, nextChildBaseRow);
}

size_t assignRowsForBlock(
    const LayoutBlock& block,
    size_t baseRow,
    const LayoutEdges& edges,
    std::unordered_map<SdfGraphNodeId, size_t>& rowByNode)
{
    const std::unordered_set<SdfGraphNodeId> blockNodes = nodeSetFor(block.nodes);
    std::unordered_set<SdfGraphNodeId> visiting;
    size_t nextBaseRow = baseRow;
    for (const SdfGraphNodeId anchor : block.anchors) {
        nextBaseRow = assignDfsRows(anchor, nextBaseRow, edges, blockNodes, rowByNode, visiting);
    }
    for (const SdfGraphNodeId id : block.nodes) {
        if (rowByNode.find(id) == rowByNode.end()) {
            nextBaseRow = assignDfsRows(id, nextBaseRow, edges, blockNodes, rowByNode, visiting);
        }
    }
    return std::max(baseRow + 1, nextBaseRow);
}

std::vector<LayoutBlock> buildLayoutBlocks(const SdfGraph& graph, const std::vector<SdfGraphNodeId>& ids, const LayoutEdges& edges, bool selectedOnly)
{
    std::vector<LayoutBlock> blocks;
    std::unordered_set<SdfGraphNodeId> remaining = nodeSetFor(ids);

    const SdfGraphNodeId output = graph.outputNode();
    if (!selectedOnly && remaining.find(output) != remaining.end()) {
        LayoutBlock block;
        block.anchors = {output};
        block.nodes = collectIncomingReachable(block.anchors, edges, remaining);
        for (const SdfGraphNodeId id : block.nodes) {
            remaining.erase(id);
        }
        blocks.push_back(std::move(block));
    }

    while (!remaining.empty()) {
        std::vector<SdfGraphNodeId> anchors = sinkNodesInSet(remaining, edges);
        if (anchors.empty()) {
            anchors.push_back(*std::min_element(remaining.begin(), remaining.end()));
        }

        LayoutBlock block;
        block.anchors = {anchors.front()};
        block.nodes = collectIncomingReachable(block.anchors, edges, remaining);
        if (block.nodes.empty()) {
            block.nodes = block.anchors;
        }
        for (const SdfGraphNodeId id : block.nodes) {
            remaining.erase(id);
        }
        blocks.push_back(std::move(block));
    }

    return blocks;
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
    std::vector<LayoutBlock> blocks = buildLayoutBlocks(graph, ids, edges, selectedOnly);
    if (blocks.empty()) {
        return false;
    }
    for (LayoutBlock& block : blocks) {
        assignColumnsForBlock(edges, block, columnByNode);
        block.maxColumn = maxColumnForBlock(block.nodes, columnByNode);
    }

    std::unordered_map<SdfGraphNodeId, size_t> rowByNode;
    size_t nextBaseRow = 0;
    for (LayoutBlock& block : blocks) {
        const size_t blockStartRow = nextBaseRow;
        nextBaseRow = assignRowsForBlock(block, blockStartRow, edges, rowByNode);
        block.rowCount = std::max<size_t>(1, nextBaseRow - blockStartRow);
    }

    const ImVec2 center = visibleCanvasCenterGraph(frame);
    const float columnSpacing = NODE_WIDTH + horizontalGap;
    const float totalWidth = static_cast<float>(blocks.front().maxColumn) * columnSpacing;
    const float startX = center.x - totalWidth * 0.5f;
    const float nodeHeight = standardNodeHeight(graph, ids);
    const size_t occupiedRows = std::max<size_t>(1, nextBaseRow);
    const float totalHeight = static_cast<float>(occupiedRows) * (nodeHeight + verticalGap);
    const float topY = center.y - totalHeight * 0.5f;

    bool changed = false;
    for (const LayoutBlock& block : blocks) {
        for (const SdfGraphNodeId id : block.nodes) {
            const auto column = columnByNode.find(id);
            const auto row = rowByNode.find(id);
            SdfGraphNode* node = graph.node(id);
            if (node == nullptr || column == columnByNode.end() || row == rowByNode.end()) {
                continue;
            }
            const float x = startX + static_cast<float>(column->second) * columnSpacing;
            const float y = topY + static_cast<float>(row->second) * (nodeHeight + verticalGap);
            changed = setNodePosition(*node, x, y) || changed;
        }
    }

    return changed;
}

} // namespace sdf3d::node_editor
