#include "sdf3d/scene/GraphMigrator.h"

#include <vector>

namespace sdf3d {
namespace {

bool isPrimitiveNode(SdfNodeType type)
{
    return type == SdfNodeType::Sphere
        || type == SdfNodeType::Box
        || type == SdfNodeType::Cylinder
        || type == SdfNodeType::Torus
        || type == SdfNodeType::Plane
        || type == SdfNodeType::Capsule
        || type == SdfNodeType::Cone
        || type == SdfNodeType::RoundBox;
}

bool alreadyFeedsMaterialOverride(const SdfGraph& graph, SdfGraphNodeId node)
{
    for (const SdfGraphLink& link : graph.links()) {
        const SdfGraphNode* target = graph.node(link.toNode);
        if (link.fromNode == node
            && link.fromSocket == "sdf"
            && target != nullptr
            && target->payload.type == SdfNodeType::MaterialOverride) {
            return true;
        }
    }

    return false;
}

} // namespace

bool GraphMigrator::injectMaterialOverrides(SdfGraph& graph)
{
    std::vector<SdfGraphNodeId> primitiveNodes;
    for (const auto& [id, node] : graph.nodes()) {
        if (isPrimitiveNode(node.payload.type) && !alreadyFeedsMaterialOverride(graph, id)) {
            primitiveNodes.push_back(id);
        }
    }

    bool changed = false;
    for (SdfGraphNodeId primitiveId : primitiveNodes) {
        SdfGraphNode* primitive = graph.node(primitiveId);
        if (primitive == nullptr) {
            continue;
        }

        const SdfMaterial material = primitive->payload.material;
        const std::vector<SdfGraphLink> oldLinks = graph.links();
        const SdfGraphNodeId materialId = graph.createNode(SdfNodeType::MaterialOverride, "Material Override");
        SdfGraphNode* materialNode = graph.node(materialId);
        if (materialNode == nullptr) {
            continue;
        }

        materialNode->payload.material = material;
        materialNode->editorX = primitive->editorX + 260.0f;
        materialNode->editorY = primitive->editorY;

        for (const SdfGraphLink& link : oldLinks) {
            if (link.fromNode == primitiveId && link.fromSocket == "sdf") {
                graph.unlink(link.fromNode, link.fromSocket, link.toNode, link.toSocket);
                graph.link(materialId, "sdf", link.toNode, link.toSocket);
            }
        }

        graph.link(primitiveId, "sdf", materialId, "sdf");
        changed = true;
    }

    return changed;
}

} // namespace sdf3d
