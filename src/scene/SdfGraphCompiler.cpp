#include "sdf3d/scene/SdfGraphCompiler.h"

#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/systems/GraphSystem.h"

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
    if (socket == "sdf") {
        return 0;
    }
    if (socket == "inputs") {
        return 0;
    }
    if (socket == "material") {
        return 1;
    }
    if (socket == "left" || socket == "base") {
        return 0;
    }
    if (socket == "right" || socket == "cutter") {
        return 1;
    }

    return 100;
}

SdfGraphLowerResult lowerSdfGraphToTreeInternal(
    const SdfGraph& graph,
    const GraphGroupRegistry* groups,
    std::unordered_map<GroupDefId, SdfNodePtr>* loweredGroups,
    std::unordered_set<GroupDefId>* visitingGroups,
    GroupDefId stableIdScope)
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
        const uint64_t rawStableId = graphNode->payload.stableId != 0 ? graphNode->payload.stableId : graphNode->id;
        node->stableId = scopedSdfNodeStableId(stableIdScope, rawStableId);
        node->parameters = graphNode->payload.parameters;
        node->materialId = graphNode->payload.materialId;
        node->groupDefinitionId = graphNode->payload.groupDefinitionId;
        node->material = graphNode->payload.material;
        if (node->type == SdfNodeType::MaterialOverride && node->materialId != 0) {
            if (const MaterialDefinition* material = graph.materials().material(node->materialId)) {
                node->material = material->material;
            }
        }

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

        std::vector<std::string> validSockets;
        if (node->type == SdfNodeType::Group) {
            if (groups == nullptr || loweredGroups == nullptr || visitingGroups == nullptr || node->groupDefinitionId == 0) {
                result.errors.push_back("Group node references a missing definition.");
            } else if (visitingGroups->find(node->groupDefinitionId) != visitingGroups->end()) {
                result.errors.push_back("Cycle detected in graph group definitions.");
            } else {
                auto loweredGroup = loweredGroups->find(node->groupDefinitionId);
                if (loweredGroup == loweredGroups->end()) {
                    const GraphGroupDefinition* definition = groups->definition(node->groupDefinitionId);
                    if (definition == nullptr) {
                        result.errors.push_back("Group node references a missing definition.");
                    } else {
                        visitingGroups->insert(node->groupDefinitionId);
                        SdfGraphLowerResult lowered = lowerSdfGraphToTreeInternal(definition->subgraph, groups, loweredGroups, visitingGroups, node->groupDefinitionId);
                        visitingGroups->erase(node->groupDefinitionId);
                        result.errors.insert(result.errors.end(), lowered.errors.begin(), lowered.errors.end());
                        loweredGroup = loweredGroups->emplace(node->groupDefinitionId, lowered.root).first;
                    }
                }
                if (loweredGroup != loweredGroups->end() && loweredGroup->second) {
                    node->children.push_back(loweredGroup->second);
                }
            }
        }

        for (const SdfGraphLink& link : inputs) {
            SdfNodePtr child = buildTree(link.fromNode);
            if (child) {
                validSockets.push_back(link.toSocket);
                node->children.push_back(std::move(child));
            }
        }

        visiting.erase(id);
        if (!GraphSystem::loweredNodeHasRequiredInputs(node->type, validSockets, node->children.size())) {
            result.errors.push_back(graphNode->payload.name + " node has no valid required input.");
            return nullptr;
        }
        loweredNodes[id] = node;
        return node;
    };

    result.root = buildTree(graphRoot);
    return result;
}

} // namespace

SdfGraphLowerResult lowerSdfGraphToTree(const SdfGraph& graph)
{
    return lowerSdfGraphToTreeInternal(graph, nullptr, nullptr, nullptr, 0);
}

SdfGraphLowerResult lowerSdfGraphToTree(const SdfGraph& graph, const GraphGroupRegistry& groups)
{
    std::unordered_map<GroupDefId, SdfNodePtr> loweredGroups;
    std::unordered_set<GroupDefId> visitingGroups;
    return lowerSdfGraphToTreeInternal(graph, &groups, &loweredGroups, &visitingGroups, 0);
}

} // namespace sdf3d
