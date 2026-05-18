#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "sdf3d/scene/SdfRotationParams.h"
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

int axisIndexFor(const SdfNode& node, float defaultAxis)
{
    const float rawAxis = glsl_emitter::parameterOr(node, "axis", defaultAxis);
    return static_cast<int>(std::clamp(rawAxis, 0.0f, 2.0f) + 0.5f);
}

std::string rotatePointAroundAxis(const std::string& pointExpr, int axis, const std::string& c, const std::string& s)
{
    if (axis == 0) {
        return "vec3(" + pointExpr + ".x, " + c + " * " + pointExpr + ".y - " + s + " * " + pointExpr + ".z, "
            + s + " * " + pointExpr + ".y + " + c + " * " + pointExpr + ".z)";
    }
    if (axis == 1) {
        return "vec3(" + c + " * " + pointExpr + ".x - " + s + " * " + pointExpr + ".z, "
            + pointExpr + ".y, " + s + " * " + pointExpr + ".x + " + c + " * " + pointExpr + ".z)";
    }
    return "vec3(" + c + " * " + pointExpr + ".x - " + s + " * " + pointExpr + ".y, "
        + s + " * " + pointExpr + ".x + " + c + " * " + pointExpr + ".y, " + pointExpr + ".z)";
}

std::string repeatedPointFor(const SdfNode& node, const std::string& pointExpr)
{
    const float x = std::max(glsl_emitter::parameterOr(node, "x", 2.0f), 0.0001f);
    const float y = std::max(glsl_emitter::parameterOr(node, "y", 2.0f), 0.0001f);
    const float z = std::max(glsl_emitter::parameterOr(node, "z", 2.0f), 0.0001f);
    const bool repeatX = glsl_emitter::parameterOr(node, "repeatX", 1.0f) >= 0.5f;
    const bool repeatY = glsl_emitter::parameterOr(node, "repeatY", 1.0f) >= 0.5f;
    const bool repeatZ = glsl_emitter::parameterOr(node, "repeatZ", 1.0f) >= 0.5f;
    const std::string cell = glsl_emitter::glslVec3(x, y, z);

    if (repeatX && repeatY && repeatZ) {
        return "(mod(" + pointExpr + " + 0.5 * " + cell + ", " + cell + ") - 0.5 * " + cell + ")";
    }
    if (!repeatX && !repeatY && !repeatZ) {
        return pointExpr;
    }

    const std::string xSize = glsl_emitter::glslFloat(x);
    const std::string ySize = glsl_emitter::glslFloat(y);
    const std::string zSize = glsl_emitter::glslFloat(z);
    const std::string xExpr = repeatX ? "(mod(" + pointExpr + ".x + 0.5 * " + xSize + ", " + xSize + ") - 0.5 * " + xSize + ")" : pointExpr + ".x";
    const std::string yExpr = repeatY ? "(mod(" + pointExpr + ".y + 0.5 * " + ySize + ", " + ySize + ") - 0.5 * " + ySize + ")" : pointExpr + ".y";
    const std::string zExpr = repeatZ ? "(mod(" + pointExpr + ".z + 0.5 * " + zSize + ", " + zSize + ") - 0.5 * " + zSize + ")" : pointExpr + ".z";
    return "vec3(" + xExpr + ", " + yExpr + ", " + zExpr + ")";
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
        const std::string translate = node->stableId != 0
            ? glslNodeParam0(node->stableId, glslVec4(x, y, z, 0.0f)) + ".xyz"
            : glslVec3(x, y, z);
        const std::string translatedPoint = "(" + pointExpr + " - " + translate + ")";
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
            return defaultMaterial();
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
            return defaultMaterial();
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
            return defaultMaterial();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Mirror node ignores extra children.");
        }

        const bool mirrorX = parameterOr(*node, "x", 1.0f) >= 0.5f;
        const bool mirrorY = parameterOr(*node, "y", 0.0f) >= 0.5f;
        const bool mirrorZ = parameterOr(*node, "z", 0.0f) >= 0.5f;
        const std::string xExpr = mirrorX ? "abs(" + pointExpr + ".x)" : pointExpr + ".x";
        const std::string yExpr = mirrorY ? "abs(" + pointExpr + ".y)" : pointExpr + ".y";
        const std::string zExpr = mirrorZ ? "abs(" + pointExpr + ".z)" : pointExpr + ".z";
        const std::string mirroredPoint = "vec3(" + xExpr + ", " + yExpr + ", " + zExpr + ")";
        return emitMaterialFor(node->children.front(), mirroredPoint, result, sdfHelpers);
    }

    case SdfNodeType::Twist: {
        if (node->children.empty()) {
            result.errors.push_back("Twist node has no child.");
            return defaultMaterial();
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
            return defaultMaterial();
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
