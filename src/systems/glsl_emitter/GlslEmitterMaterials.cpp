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

std::string smoothnessExpr(const SdfNode& node, GlslEmitMode mode, uint64_t nodeId)
{
    const float smoothness = std::max(parameterOr(node, "smoothness", 0.25f), 0.0001f);
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return glslFloat(smoothness);
    }
    return "max(" + glslNodeParamComponent(mode, nodeId, glslVec4(smoothness, 0.0f, 0.0f, 0.0f), 'x') + ", 0.000100)";
}

std::string strengthExpr(const SdfNode& node, GlslEmitMode mode, uint64_t nodeId, float fallback)
{
    const float strength = parameterOr(node, "strength", fallback);
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return glslFloat(strength);
    }
    return glslNodeParamComponent(mode, nodeId, glslVec4(strength, 0.0f, 0.0f, 0.0f), 'x');
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
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode)
{
    if (!node) {
        result.errors.push_back("Encountered a null SDF node while emitting material evaluation.");
        return defaultMaterial(pointExpr);
    }

    switch (node->type) {
    case SdfNodeType::SolidMaterial:
    case SdfNodeType::CheckerMaterial:
    case SdfNodeType::ValueNoiseMaterial:
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
            return emitMaterialFor(node->children[1], pointExpr, result, sdfHelpers, mode);
        }
        const int materialId = materialSystem.appendMaterial(result, node->material);
        return sampleMaterialCall(materialId, pointExpr);
    }
    case SdfNodeType::Group:
        if (node->children.empty()) {
            result.errors.push_back("Group node references a missing definition.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Group node ignores extra children.");
        }
        return emitMaterialFor(node->children.front(), pointExpr, result, sdfHelpers, mode);
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
        const std::string translate = glslNodeParam0(mode, node->stableId, glslVec4(x, y, z, 0.0f)) + ".xyz";
        const std::string translatedPoint = "(" + pointExpr + " - " + translate + ")";
        return emitMaterialFor(node->children.front(), translatedPoint, result, sdfHelpers, mode);
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
        const std::string rotation = glslNodeParam0(mode, node->stableId, glslVec4(fallback.x, fallback.y, fallback.z, fallback.w));
        const std::string rotatedPoint = "(transpose(sdf3d_rotationQuat(" + rotation + ")) * " + pointExpr + ")";
        return emitMaterialFor(node->children.front(), rotatedPoint, result, sdfHelpers, mode);
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
        const std::string scaleParam = glslNodeParam0(mode, node->stableId, glslVec4(x, y, z, distanceScale));
        const std::string scale = "(" + scaleParam + ".xyz)";
        const std::string scaledPoint = "(" + pointExpr + " / " + scale + ")";
        return emitMaterialFor(node->children.front(), scaledPoint, result, sdfHelpers, mode);
    }
    case SdfNodeType::Repeat: {
        if (node->children.empty()) {
            result.errors.push_back("Repeat node has no child.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Repeat node ignores extra children.");
        }

        const std::string repeatedPoint = repeatedPointFor(*node, node->stableId, mode, pointExpr);
        return emitMaterialFor(node->children.front(), repeatedPoint, result, sdfHelpers, mode);
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
        return emitMaterialFor(node->children.front(), mirroredPoint, result, sdfHelpers, mode);
    }
    case SdfNodeType::Twist: {
        if (node->children.empty()) {
            result.errors.push_back("Twist node has no child.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Twist node ignores extra children.");
        }

        const std::string strength = strengthExpr(*node, mode, node->stableId, 1.0f);
        const int axis = axisIndexFor(*node, 1.0f);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        const std::string twistedPoint = rotatePointAroundAxis(pointExpr, axis, c, s);
        return emitMaterialFor(node->children.front(), twistedPoint, result, sdfHelpers, mode);
    }
    case SdfNodeType::Bend: {
        if (node->children.empty()) {
            result.errors.push_back("Bend node has no child.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Bend node ignores extra children.");
        }

        const std::string strength = strengthExpr(*node, mode, node->stableId, 0.5f);
        const int axis = axisIndexFor(*node, 0.0f);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        const std::string bentPoint = rotatePointAroundAxis(pointExpr, axis, c, s);
        return emitMaterialFor(node->children.front(), bentPoint, result, sdfHelpers, mode);
    }
    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion: {
        if (node->children.empty()) {
            result.errors.push_back(glslNodeTypeName(node->type) + " node has no children.");
            return defaultMaterial(pointExpr);
        }

        const bool smooth = node->type == SdfNodeType::SmoothUnion;
        const std::string smoothness = smoothnessExpr(*node, mode, node->stableId);
        std::string distance = helperDistanceFor(node->children.front(), pointExpr, result, sdfHelpers);
        std::string material = emitMaterialFor(node->children.front(), pointExpr, result, sdfHelpers, mode);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string childDistance = helperDistanceFor(node->children[i], pointExpr, result, sdfHelpers);
            const std::string childMaterial = emitMaterialFor(node->children[i], pointExpr, result, sdfHelpers, mode);
            if (smooth) {
                const std::string blend = "clamp(0.5 + 0.5 * (" + childDistance + " - " + distance + ") / " + smoothness + ", 0.0, 1.0)";
                material = "mixMaterial(" + childMaterial + ", " + material + ", " + blend + ")";
                distance = "sdf3d_smin(" + distance + ", " + childDistance + ", " + smoothness + ")";
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
        return emitMaterialFor(node->children.front(), pointExpr, result, sdfHelpers, mode);
    }
    case SdfNodeType::SmoothSubtract: {
        if (node->children.empty()) {
            result.errors.push_back("SmoothSubtract node requires a base child.");
            return defaultMaterial(pointExpr);
        }
        if (node->children.size() == 1) {
            result.errors.push_back("SmoothSubtract node is missing a cutter child; bypassing to base.");
            return emitMaterialFor(node->children.front(), pointExpr, result, sdfHelpers, mode);
        }
        if (node->children.size() > 2) {
            result.errors.push_back("SmoothSubtract node ignores extra children beyond base and cutter.");
        }

        const std::string smoothness = smoothnessExpr(*node, mode, node->stableId);
        const std::string baseDistance = helperDistanceFor(node->children[0], pointExpr, result, sdfHelpers);
        const std::string cutterDistance = helperDistanceFor(node->children[1], pointExpr, result, sdfHelpers);
        const std::string baseMaterial = emitMaterialFor(node->children[0], pointExpr, result, sdfHelpers, mode);
        const std::string cutterMaterial = emitMaterialFor(node->children[1], pointExpr, result, sdfHelpers, mode);
        const std::string blend = "clamp(0.5 + 0.5 * (" + cutterDistance + " + " + baseDistance + ") / " + smoothness + ", 0.0, 1.0)";
        return "mixMaterial(" + cutterMaterial + ", " + baseMaterial + ", " + blend + ")";
    }
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect: {
        if (node->children.empty()) {
            result.errors.push_back(glslNodeTypeName(node->type) + " node has no children.");
            return defaultMaterial(pointExpr);
        }

        const bool smooth = node->type == SdfNodeType::SmoothIntersect;
        const std::string smoothness = smoothnessExpr(*node, mode, node->stableId);
        std::string distance = helperDistanceFor(node->children.front(), pointExpr, result, sdfHelpers);
        std::string material = emitMaterialFor(node->children.front(), pointExpr, result, sdfHelpers, mode);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string childDistance = helperDistanceFor(node->children[i], pointExpr, result, sdfHelpers);
            const std::string childMaterial = emitMaterialFor(node->children[i], pointExpr, result, sdfHelpers, mode);
            if (smooth) {
                const std::string blend = "clamp(0.5 + 0.5 * (" + distance + " - " + childDistance + ") / " + smoothness + ", 0.0, 1.0)";
                material = "mixMaterial(" + childMaterial + ", " + material + ", " + blend + ")";
                distance = "(-sdf3d_smin(-(" + distance + "), -(" + childDistance + "), " + smoothness + "))";
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
