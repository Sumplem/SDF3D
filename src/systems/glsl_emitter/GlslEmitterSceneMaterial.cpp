#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "sdf3d/systems/GlslNodeNames.h"
#include "sdf3d/systems/MaterialSystem.h"

#include <algorithm>

namespace sdf3d {
namespace {

constexpr int kDefaultMaterialId = 0;

std::string sampleMaterialCall(int materialId)
{
    return "sampleMaterial(" + std::to_string(materialId) + ")";
}

std::string defaultMaterial()
{
    return sampleMaterialCall(kDefaultMaterialId);
}

std::string helperDistanceFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers)
{
    const auto it = sdfHelpers.functionNameByNode.find(node.get());
    if (it == sdfHelpers.functionNameByNode.end()) {
        result.errors.push_back("Missing SDF helper for material evaluation.");
        return "1e6";
    }

    return it->second + "(" + pointExpr + ")";
}

std::string emitMaterialFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers)
{
    using namespace glsl_emitter;

    if (!node) {
        result.errors.push_back("Encountered a null SDF node while emitting material evaluation.");
        return defaultMaterial();
    }

    switch (node->type) {
    case SdfNodeType::Sphere:
    case SdfNodeType::Box:
    case SdfNodeType::Cylinder:
    case SdfNodeType::Torus:
    case SdfNodeType::Plane:
        return defaultMaterial();

    case SdfNodeType::MaterialOverride: {
        if (node->children.empty()) {
            result.errors.push_back("MaterialOverride node has no SDF input.");
            return defaultMaterial();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("MaterialOverride node ignores extra children.");
        }

        const MaterialSystem materialSystem;
        const int materialId = materialSystem.appendMaterial(result, node->material);
        return sampleMaterialCall(materialId);
    }

    case SdfNodeType::Translate: {
        if (node->children.empty()) {
            result.errors.push_back("Translate node has no child.");
            return defaultMaterial();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Translate node ignores extra children.");
        }

        const float x = parameterOr(*node, "x", 0.0f);
        const float y = parameterOr(*node, "y", 0.0f);
        const float z = parameterOr(*node, "z", 0.0f);
        const std::string translatedPoint = "(" + pointExpr + " - " + glslVec3(x, y, z) + ")";
        return emitMaterialFor(node->children.front(), translatedPoint, result, sdfHelpers);
    }

    case SdfNodeType::Rotate: {
        if (node->children.empty()) {
            result.errors.push_back("Rotate node has no child.");
            return defaultMaterial();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Rotate node ignores extra children.");
        }

        const float x = parameterOr(*node, "xDegrees", 0.0f);
        const float y = parameterOr(*node, "yDegrees", 0.0f);
        const float z = parameterOr(*node, "zDegrees", 0.0f);
        const std::string rotatedPoint = "(transpose(sdf3d_rotationXYZ(" + glslVec3(x, y, z) + ")) * " + pointExpr + ")";
        return emitMaterialFor(node->children.front(), rotatedPoint, result, sdfHelpers);
    }

    case SdfNodeType::Scale: {
        if (node->children.empty()) {
            result.errors.push_back("Scale node has no child.");
            return defaultMaterial();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Scale node ignores extra children.");
        }

        const float scale = std::max(parameterOr(*node, "scale", 1.0f), 0.0001f);
        const std::string scaledPoint = "(" + pointExpr + " / " + glslFloat(scale) + ")";
        return emitMaterialFor(node->children.front(), scaledPoint, result, sdfHelpers);
    }

    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion: {
        if (node->children.empty()) {
            result.errors.push_back(glslNodeTypeName(node->type) + " node has no children.");
            return defaultMaterial();
        }

        const bool smooth = node->type == SdfNodeType::SmoothUnion;
        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        std::string distance = helperDistanceFor(node->children.front(), pointExpr, result, sdfHelpers);
        std::string material = emitMaterialFor(node->children.front(), pointExpr, result, sdfHelpers);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string childDistance = helperDistanceFor(node->children[i], pointExpr, result, sdfHelpers);
            const std::string childMaterial = emitMaterialFor(node->children[i], pointExpr, result, sdfHelpers);
            if (smooth) {
                const std::string blend = "clamp(0.5 + 0.5 * (" + childDistance + " - " + distance + ") / " + glslFloat(smoothness) + ", 0.0, 1.0)";
                material = "mixMaterial(" + childMaterial + ", " + material + ", " + blend + ")";
                distance = "sdf3d_smin(" + distance + ", " + childDistance + ", " + glslFloat(smoothness) + ")";
            } else {
                material = "selectMaterial(" + distance + " < " + childDistance + ", " + material + ", " + childMaterial + ")";
                distance = "min(" + distance + ", " + childDistance + ")";
            }
        }
        return material;
    }

    case SdfNodeType::Subtract: {
        if (node->children.empty()) {
            result.errors.push_back("Subtract node requires a base child.");
            return defaultMaterial();
        }
        if (node->children.size() == 1) {
            result.errors.push_back("Subtract node is missing a cutter child; bypassing to base.");
        }
        if (node->children.size() > 2) {
            result.errors.push_back("Subtract node ignores extra children beyond base and cutter.");
        }
        return emitMaterialFor(node->children.front(), pointExpr, result, sdfHelpers);
    }

    case SdfNodeType::SmoothSubtract: {
        if (node->children.empty()) {
            result.errors.push_back("SmoothSubtract node requires a base child.");
            return defaultMaterial();
        }
        if (node->children.size() == 1) {
            result.errors.push_back("SmoothSubtract node is missing a cutter child; bypassing to base.");
            return emitMaterialFor(node->children.front(), pointExpr, result, sdfHelpers);
        }
        if (node->children.size() > 2) {
            result.errors.push_back("SmoothSubtract node ignores extra children beyond base and cutter.");
        }

        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        const std::string baseDistance = helperDistanceFor(node->children[0], pointExpr, result, sdfHelpers);
        const std::string cutterDistance = helperDistanceFor(node->children[1], pointExpr, result, sdfHelpers);
        const std::string baseMaterial = emitMaterialFor(node->children[0], pointExpr, result, sdfHelpers);
        const std::string cutterMaterial = emitMaterialFor(node->children[1], pointExpr, result, sdfHelpers);
        const std::string blend = "clamp(0.5 + 0.5 * (" + cutterDistance + " + " + baseDistance + ") / " + glslFloat(smoothness) + ", 0.0, 1.0)";
        return "mixMaterial(" + cutterMaterial + ", " + baseMaterial + ", " + blend + ")";
    }

    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect: {
        if (node->children.empty()) {
            result.errors.push_back(glslNodeTypeName(node->type) + " node has no children.");
            return defaultMaterial();
        }

        const bool smooth = node->type == SdfNodeType::SmoothIntersect;
        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        std::string distance = helperDistanceFor(node->children.front(), pointExpr, result, sdfHelpers);
        std::string material = emitMaterialFor(node->children.front(), pointExpr, result, sdfHelpers);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string childDistance = helperDistanceFor(node->children[i], pointExpr, result, sdfHelpers);
            const std::string childMaterial = emitMaterialFor(node->children[i], pointExpr, result, sdfHelpers);
            if (smooth) {
                const std::string blend = "clamp(0.5 + 0.5 * (" + distance + " - " + childDistance + ") / " + glslFloat(smoothness) + ", 0.0, 1.0)";
                material = "mixMaterial(" + childMaterial + ", " + material + ", " + blend + ")";
                distance = "(-sdf3d_smin(-(" + distance + "), -(" + childDistance + "), " + glslFloat(smoothness) + "))";
            } else {
                material = "selectMaterial(" + distance + " > " + childDistance + ", " + material + ", " + childMaterial + ")";
                distance = "max(" + distance + ", " + childDistance + ")";
            }
        }
        return material;
    }

    default:
        result.errors.push_back("Unsupported SDF node type in material evaluation: " + glslNodeTypeName(node->type));
        return defaultMaterial();
    }
}

} // namespace

std::string GlslEmitter::emitSceneMaterialExpression(
    const SdfNodePtr& root,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers) const
{
    const MaterialSystem materialSystem;
    materialSystem.ensureDefaultMaterial(result);
    return emitMaterialFor(root, pointExpr, result, sdfHelpers);
}

} // namespace sdf3d
