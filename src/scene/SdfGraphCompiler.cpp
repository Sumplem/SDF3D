#include "sdf3d/scene/SdfGraphCompiler.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace sdf3d {
namespace {

int socketOrder(const std::string& socket)
{
    if (socket == "child") {
        return 0;
    }
    if (socket == "left" || socket == "base") {
        return 0;
    }
    if (socket == "right" || socket == "cutter") {
        return 1;
    }

    return 100;
}

} // namespace

SdfGraphLowerResult lowerSdfGraphToTree(const SdfGraph& graph)
{
    SdfGraphLowerResult result;
    if (graph.outputNode() == 0) {
        result.errors.push_back("Output node has no connected surface input.");
        return result;
    }

    SdfGraphNodeId graphRoot = graph.outputNode();
    if (const SdfGraphNode* outputNode = graph.node(graphRoot)) {
        if (outputNode->payload.type == SdfNodeType::Output) {
            graphRoot = 0;
            for (const SdfGraphLink& link : graph.links()) {
                if (link.toNode == outputNode->id && link.toSocket == "surface") {
                    graphRoot = link.fromNode;
                    break;
                }
            }

            if (graphRoot == 0) {
                result.errors.push_back("Output node has no connected surface input.");
                return result;
            }
        }
    }

    std::unordered_map<SdfGraphNodeId, SdfNodePtr> loweredNodes;
    std::unordered_set<SdfGraphNodeId> visiting;

    std::function<SdfNodePtr(SdfGraphNodeId)> buildTree = [&](SdfGraphNodeId id) -> SdfNodePtr {
        if (loweredNodes.find(id) != loweredNodes.end()) {
            return loweredNodes[id];
        }

        if (visiting.find(id) != visiting.end()) {
            result.errors.push_back("Cycle detected in SDF graph.");
            return nullptr;
        }

        const SdfGraphNode* graphNode = graph.node(id);
        if (graphNode == nullptr) {
            result.errors.push_back("Graph references a missing SDF node.");
            return nullptr;
        }

        visiting.insert(id);

        SdfNodePtr node = makeSdfNode(graphNode->payload.type, graphNode->payload.name);
        node->parameters = graphNode->payload.parameters;
        node->material = graphNode->payload.material;

        std::vector<SdfGraphLink> inputs;
        for (const SdfGraphLink& link : graph.links()) {
            if (link.toNode == id) {
                inputs.push_back(link);
            }
        }

        // AGENT: Named sockets map user-facing graph links to expression order;
        // fallback lexical ordering keeps custom socket names deterministic.
        std::sort(inputs.begin(), inputs.end(), [](const SdfGraphLink& a, const SdfGraphLink& b) {
            const int orderA = socketOrder(a.toSocket);
            const int orderB = socketOrder(b.toSocket);
            if (orderA != orderB) {
                return orderA < orderB;
            }
            if (a.toSocket != b.toSocket) {
                return a.toSocket < b.toSocket;
            }
            return a.fromNode < b.fromNode;
        });

        for (const SdfGraphLink& link : inputs) {
            SdfNodePtr child = buildTree(link.fromNode);
            if (child) {
                node->children.push_back(std::move(child));
            }
        }

        visiting.erase(id);
        loweredNodes[id] = node;
        return node;
    };

    result.root = buildTree(graphRoot);
    return result;
}

} // namespace sdf3d
