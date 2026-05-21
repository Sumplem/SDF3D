#include "sdf3d/systems/MaterialSystem.h"

#include "sdf3d/scene/SdfGraphCompiler.h"
#include "sdf3d/systems/GlslNodeNames.h"

namespace sdf3d {
namespace {

void collectNodeMaterials(const SdfNodePtr& node, SdfCompileResult& result, const MaterialSystem& materialSystem)
{
    if (!node) {
        result.errors.push_back("Encountered a null SDF node while collecting materials.");
        return;
    }

    switch (node->type) {
    case SdfNodeType::Sphere:
    case SdfNodeType::Box:
    case SdfNodeType::Cylinder:
    case SdfNodeType::Torus:
    case SdfNodeType::Plane:
        return;

    case SdfNodeType::SolidMaterial:
    case SdfNodeType::CheckerMaterial:
    case SdfNodeType::ValueNoiseMaterial:
        materialSystem.appendMaterial(result, node->material);
        return;

    case SdfNodeType::MaterialOverride:
        if (node->children.empty()) {
            result.errors.push_back("MaterialOverride node has no SDF input.");
        }
        if (node->children.size() > 1) {
            collectNodeMaterials(node->children[1], result, materialSystem);
            return;
        }
        materialSystem.appendMaterial(result, node->material);
        return;

    case SdfNodeType::Group:
        if (node->children.empty()) {
            result.errors.push_back("Group node references a missing definition.");
            return;
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Group node ignores extra children.");
        }
        collectNodeMaterials(node->children.front(), result, materialSystem);
        return;

    case SdfNodeType::Translate:
    case SdfNodeType::Rotate:
    case SdfNodeType::Scale:
        if (node->children.empty()) {
            result.errors.push_back(glslNodeTypeName(node->type) + " node has no child.");
            return;
        }
        if (node->children.size() > 1) {
            result.errors.push_back(glslNodeTypeName(node->type) + " node ignores extra children.");
        }
        collectNodeMaterials(node->children.front(), result, materialSystem);
        return;

    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion:
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect:
        if (node->children.empty()) {
            result.errors.push_back(glslNodeTypeName(node->type) + " node has no children.");
            return;
        }
        for (const SdfNodePtr& child : node->children) {
            collectNodeMaterials(child, result, materialSystem);
        }
        return;

    case SdfNodeType::Subtract:
        if (node->children.empty()) {
            result.errors.push_back("Subtract node requires a base child.");
            return;
        }
        if (node->children.size() == 1) {
            result.errors.push_back("Subtract node is missing a cutter child; bypassing to base.");
        }
        if (node->children.size() > 2) {
            result.errors.push_back("Subtract node ignores extra children beyond base and cutter.");
        }
        // AGENT: Subtract material behavior keeps base material, matching sceneMaterial GLSL.
        collectNodeMaterials(node->children.front(), result, materialSystem);
        return;

    case SdfNodeType::SmoothSubtract:
        if (node->children.empty()) {
            result.errors.push_back("SmoothSubtract node requires a base child.");
            return;
        }
        if (node->children.size() == 1) {
            result.errors.push_back("SmoothSubtract node is missing a cutter child; bypassing to base.");
        }
        if (node->children.size() > 2) {
            result.errors.push_back("SmoothSubtract node ignores extra children beyond base and cutter.");
        }
        collectNodeMaterials(node->children.front(), result, materialSystem);
        if (node->children.size() > 1) {
            collectNodeMaterials(node->children[1], result, materialSystem);
        }
        return;

    default:
        result.errors.push_back("Unsupported SDF node type while collecting materials: " + glslNodeTypeName(node->type));
        return;
    }
}

} // namespace

int MaterialSystem::ensureDefaultMaterial(SdfCompileResult& result) const
{
    if (result.materials.empty()) {
        result.materials.push_back({SdfMaterial{}});
    }

    return 0;
}

int MaterialSystem::appendMaterial(SdfCompileResult& result, const SdfMaterial& material) const
{
    ensureDefaultMaterial(result);
    const int id = static_cast<int>(result.materials.size());
    result.materials.push_back({material});
    return id;
}

SdfCompileResult MaterialSystem::collectMaterials(const SdfNodePtr& root) const
{
    SdfCompileResult result;
    ensureDefaultMaterial(result);
    if (!root) {
        result.errors.push_back("Cannot collect materials from an empty SDF tree.");
        return result;
    }

    collectNodeMaterials(root, result, *this);
    return result;
}

SdfCompileResult MaterialSystem::collectMaterials(const SdfGraph& graph) const
{
    const SdfGraphLowerResult lowered = lowerSdfGraphToTree(graph);
    SdfCompileResult result = collectMaterials(lowered.root);
    result.errors.insert(result.errors.begin(), lowered.errors.begin(), lowered.errors.end());
    return result;
}

SdfCompileResult MaterialSystem::collectMaterials(const SdfGraph& graph, const GraphGroupRegistry& groups) const
{
    const SdfGraphLowerResult lowered = lowerSdfGraphToTree(graph, groups);
    SdfCompileResult result = collectMaterials(lowered.root);
    result.errors.insert(result.errors.begin(), lowered.errors.begin(), lowered.errors.end());
    return result;
}

} // namespace sdf3d
