#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "GlslEmitterInternal.h"
#include "GlslEmitterMath.h"
#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/systems/GlslNodeNames.h"
#include "sdf3d/systems/MaterialSystem.h"

#include <algorithm>

namespace sdf3d {

std::string GlslEmitter::emitMaterialNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    using namespace glsl_emitter;

    if (node->children.empty()) {
        result.errors.push_back("MaterialOverride node has no SDF input.");
        return glslNoHit();
    }
    if (node->children.size() > 2 || (node->children.size() > 1 && !isSdfMaterialNode(node->children[1]->type))) {
        result.errors.push_back("MaterialOverride node ignores extra children.");
    }

    const MaterialSystem materialSystem;
    const SdfNodePtr materialNode = node->children.size() > 1 && node->children[1] && isSdfMaterialNode(node->children[1]->type) ? node->children[1] : nullptr;
    const int materialId = materialSystem.appendMaterial(result, materialNode ? materialNode->material : node->material);
    const std::string child = emitNode(node->children.front(), pointExpr, result);
    return "vec2(" + hitDistance(child) + ", " + glslFloat(static_cast<float>(materialId)) + ")";
}

} // namespace sdf3d

namespace sdf3d::glsl_emitter {
namespace {

constexpr int kDefaultMaterialId = 0;

std::string sampleMaterialCall(int materialId, const std::string& pointExpr)
{
    return "sampleMaterial(" + std::to_string(materialId) + ", " + pointExpr + ")";
}

std::string defaultMaterial(const std::string& pointExpr)
{
    return sampleMaterialCall(kDefaultMaterialId, pointExpr);
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

} // namespace

std::string emitMaterialGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context)
{
    if (node->children.empty()) {
        result.errors.push_back("MaterialOverride node has no SDF input.");
        return "1e6";
    }
    if (node->children.size() > 2 || (node->children.size() > 1 && !isSdfMaterialNode(node->children[1]->type))) {
        result.errors.push_back("MaterialOverride node ignores extra children.");
    }
    return helperCallFor(node->children.front(), pointExpr, context);
}

std::string emitMaterialNodeSample(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result)
{
    const MaterialSystem materialSystem;
    const int materialId = materialSystem.appendMaterial(result, node->material);
    return sampleMaterialCall(materialId, pointExpr);
}

std::string emitMaterialFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers)
{
    if (!node) {
        result.errors.push_back("Encountered a null SDF node while emitting material evaluation.");
        return defaultMaterial(pointExpr);
    }

    switch (node->type) {
    case SdfNodeType::SolidMaterial:
    case SdfNodeType::CheckerMaterial:
        return emitMaterialNodeSample(node, pointExpr, result);

    case SdfNodeType::Sphere:
    case SdfNodeType::Box:
    case SdfNodeType::Cylinder:
    case SdfNodeType::Torus:
    case SdfNodeType::Plane:
        return defaultMaterial(pointExpr);

    case SdfNodeType::MaterialOverride: {
        if (node->children.empty()) {
            result.errors.push_back("MaterialOverride node has no SDF input.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 2 || (node->children.size() > 1 && !isSdfMaterialNode(node->children[1]->type))) {
            result.errors.push_back("MaterialOverride node ignores extra children.");
        }

        const MaterialSystem materialSystem;
        if (node->children.size() > 1 && isSdfMaterialNode(node->children[1]->type)) {
            return emitMaterialFor(node->children[1], pointExpr, result, sdfHelpers);
        }
        const int materialId = materialSystem.appendMaterial(result, node->material);
        return sampleMaterialCall(materialId, pointExpr);
    }
    case SdfNodeType::Translate: {
        if (node->children.empty()) {
            result.errors.push_back("Translate node has no child.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Translate node ignores extra children.");
        }

        const float x = parameterOr(*node, "x", 0.0f);
        const float y = parameterOr(*node, "y", 0.0f);
        const float z = parameterOr(*node, "z", 0.0f);
        const std::string translate = node->stableId != 0
            ? glslNodeParam0(node->stableId, glslVec4(x, y, z, 0.0f)) + ".xyz"
            : glslVec3(x, y, z);
        const std::string translatedPoint = "(" + pointExpr + " - " + translate + ")";
        return emitMaterialFor(node->children.front(), translatedPoint, result, sdfHelpers);
    }
    case SdfNodeType::Rotate: {
        if (node->children.empty()) {
            result.errors.push_back("Rotate node has no child.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Rotate node ignores extra children.");
        }

        const glm::vec4 fallback = rotationQuaternionForNode(*node);
        const std::string rotation = node->stableId != 0
            ? glslNodeParam0(node->stableId, glslVec4(fallback.x, fallback.y, fallback.z, fallback.w))
            : glslVec4(fallback.x, fallback.y, fallback.z, fallback.w);
        const std::string rotatedPoint = "(transpose(sdf3d_rotationQuat(" + rotation + ")) * " + pointExpr + ")";
        return emitMaterialFor(node->children.front(), rotatedPoint, result, sdfHelpers);
    }
    case SdfNodeType::Scale: {
        if (node->children.empty()) {
            result.errors.push_back("Scale node has no child.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Scale node ignores extra children.");
        }

        const float uniformScale = parameterOr(*node, "scale", 1.0f);
        const float x = std::max(parameterOr(*node, "x", uniformScale), 0.0001f);
        const float y = std::max(parameterOr(*node, "y", uniformScale), 0.0001f);
        const float z = std::max(parameterOr(*node, "z", uniformScale), 0.0001f);
        const float distanceScale = std::min({x, y, z});
        const std::string scaleParam = node->stableId != 0
            ? glslNodeParam0(node->stableId, glslVec4(x, y, z, distanceScale))
            : glslVec4(x, y, z, distanceScale);
        const std::string scale = node->stableId != 0 ? "(" + scaleParam + ".xyz)" : glslVec3(x, y, z);
        const std::string scaledPoint = "(" + pointExpr + " / " + scale + ")";
        return emitMaterialFor(node->children.front(), scaledPoint, result, sdfHelpers);
    }
    case SdfNodeType::Repeat: {
        if (node->children.empty()) {
            result.errors.push_back("Repeat node has no child.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Repeat node ignores extra children.");
        }

        const std::string repeatedPoint = repeatedPointFor(*node, pointExpr);
        return emitMaterialFor(node->children.front(), repeatedPoint, result, sdfHelpers);
    }
    case SdfNodeType::Mirror: {
        if (node->children.empty()) {
            result.errors.push_back("Mirror node has no child.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Mirror node ignores extra children.");
        }

        const std::string mirroredPoint = mirroredPointFor(*node, pointExpr);
        return emitMaterialFor(node->children.front(), mirroredPoint, result, sdfHelpers);
    }
    case SdfNodeType::Twist: {
        if (node->children.empty()) {
            result.errors.push_back("Twist node has no child.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Twist node ignores extra children.");
        }

        const std::string strength = glslFloat(parameterOr(*node, "strength", 1.0f));
        const int axis = axisIndexFor(*node, 1.0f);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        const std::string twistedPoint = rotatePointAroundAxis(pointExpr, axis, c, s);
        return emitMaterialFor(node->children.front(), twistedPoint, result, sdfHelpers);
    }
    case SdfNodeType::Bend: {
        if (node->children.empty()) {
            result.errors.push_back("Bend node has no child.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Bend node ignores extra children.");
        }

        const std::string strength = glslFloat(parameterOr(*node, "strength", 0.5f));
        const int axis = axisIndexFor(*node, 0.0f);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        const std::string bentPoint = rotatePointAroundAxis(pointExpr, axis, c, s);
        return emitMaterialFor(node->children.front(), bentPoint, result, sdfHelpers);
    }
    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion: {
        if (node->children.empty()) {
            result.errors.push_back(glslNodeTypeName(node->type) + " node has no children.");
            return defaultMaterial(pointExpr);
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
            return defaultMaterial(pointExpr);
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
            return defaultMaterial(pointExpr);
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
            return defaultMaterial(pointExpr);
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
        return defaultMaterial(pointExpr);
    }
}

} // namespace sdf3d::glsl_emitter
