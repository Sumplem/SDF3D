#include "GlslEmitterSdfHelperContext.h"

#include "GlslEmitterFormatting.h"
#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/systems/GlslNodeNames.h"

#include <algorithm>

namespace sdf3d::glsl_emitter {

namespace {

constexpr float warpCorrection = 1.5f;

int axisIndexFor(const SdfNode& node, float defaultAxis)
{
    const float rawAxis = parameterOr(node, "axis", defaultAxis);
    return static_cast<int>(std::clamp(rawAxis, 0.0f, 2.0f) + 0.5f);
}

} // namespace

uint64_t helperIdFor(const SdfNodePtr& node, SdfHelperEmitContext& context)
{
    if (node->stableId != 0) {
        return node->stableId;
    }

    const SdfNode* key = node.get();
    auto it = context.generatedIds.find(key);
    if (it != context.generatedIds.end()) {
        return it->second;
    }

    // AGENT: Legacy tree compiles have no graph IDs; generated IDs are isolated
    // above normal graph IDs so Step 1 remains usable before graph-only compile.
    const uint64_t id = context.nextGeneratedId++;
    context.generatedIds.emplace(key, id);
    return id;
}

std::string helperNameFor(const SdfNodePtr& node, SdfHelperEmitContext& context)
{
    return "sdf_node_" + std::to_string(helperIdFor(node, context));
}

std::string helperCallFor(const SdfNodePtr& node, const std::string& pointExpr, SdfHelperEmitContext& context)
{
    return helperNameFor(node, context) + "(" + pointExpr + ")";
}

std::string emitGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context)
{
    switch (node->type) {
    case SdfNodeType::Sphere: {
        const float radius = parameterOr(*node, "radius", 1.0f);
        return "(length(" + pointExpr + ") - " + glslFloat(radius) + ")";
    }
    case SdfNodeType::Box: {
        result.usesBox = true;
        const float x = parameterOr(*node, "x", 1.0f);
        const float y = parameterOr(*node, "y", 1.0f);
        const float z = parameterOr(*node, "z", 1.0f);
        return "sdf3d_box(" + pointExpr + ", " + glslVec3(x, y, z) + ")";
    }
    case SdfNodeType::Cylinder: {
        result.usesCylinder = true;
        const float radius = parameterOr(*node, "radius", 1.0f);
        const float halfHeight = parameterOr(*node, "halfHeight", 1.0f);
        return "sdf3d_cylinder(" + pointExpr + ", " + glslFloat(radius) + ", " + glslFloat(halfHeight) + ")";
    }
    case SdfNodeType::Torus: {
        const float majorRadius = parameterOr(*node, "majorRadius", 1.0f);
        const float minorRadius = parameterOr(*node, "minorRadius", 0.25f);
        return "(length(vec2(length(" + pointExpr + ".xz) - " + glslFloat(majorRadius) + ", "
            + pointExpr + ".y)) - " + glslFloat(minorRadius) + ")";
    }
    case SdfNodeType::Plane: {
        const float normalX = parameterOr(*node, "normalX", 0.0f);
        const float normalY = parameterOr(*node, "normalY", 1.0f);
        const float normalZ = parameterOr(*node, "normalZ", 0.0f);
        const float offset = parameterOr(*node, "offset", 0.0f);
        return "(dot(" + pointExpr + ", normalize(" + glslVec3(normalX, normalY, normalZ) + ")) + " + glslFloat(offset) + ")";
    }
    case SdfNodeType::Union: {
        if (node->children.empty()) {
            result.errors.push_back("Union node has no children.");
            return "1e6";
        }
        std::string expression = helperCallFor(node->children.front(), pointExpr, context);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = helperCallFor(node->children[i], pointExpr, context);
            expression = "min(" + expression + ", " + child + ")";
        }
        return expression;
    }
    case SdfNodeType::SmoothUnion: {
        if (node->children.empty()) {
            result.errors.push_back("SmoothUnion node has no children.");
            return "1e6";
        }
        result.usesSmoothMin = true;
        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        std::string expression = helperCallFor(node->children.front(), pointExpr, context);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = helperCallFor(node->children[i], pointExpr, context);
            expression = "sdf3d_smin(" + expression + ", " + child + ", " + glslFloat(smoothness) + ")";
        }
        return expression;
    }
    case SdfNodeType::Subtract: {
        if (node->children.empty()) {
            result.errors.push_back("Subtract node requires a base child.");
            return "1e6";
        }
        if (node->children.size() == 1) {
            result.errors.push_back("Subtract node is missing a cutter child; bypassing to base.");
            return helperCallFor(node->children.front(), pointExpr, context);
        }
        if (node->children.size() > 2) {
            result.errors.push_back("Subtract node ignores extra children beyond base and cutter.");
        }
        const std::string base = helperCallFor(node->children[0], pointExpr, context);
        const std::string cutter = helperCallFor(node->children[1], pointExpr, context);
        return "max(-(" + cutter + "), " + base + ")";
    }
    case SdfNodeType::SmoothSubtract: {
        if (node->children.empty()) {
            result.errors.push_back("SmoothSubtract node requires a base child.");
            return "1e6";
        }
        if (node->children.size() == 1) {
            result.errors.push_back("SmoothSubtract node is missing a cutter child; bypassing to base.");
            return helperCallFor(node->children.front(), pointExpr, context);
        }
        if (node->children.size() > 2) {
            result.errors.push_back("SmoothSubtract node ignores extra children beyond base and cutter.");
        }
        result.usesSmoothMin = true;
        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        const std::string base = helperCallFor(node->children[0], pointExpr, context);
        const std::string cutter = helperCallFor(node->children[1], pointExpr, context);
        return "(-sdf3d_smin(-(" + base + "), " + cutter + ", " + glslFloat(smoothness) + "))";
    }
    case SdfNodeType::Intersect: {
        if (node->children.empty()) {
            result.errors.push_back("Intersect node has no children.");
            return "1e6";
        }
        std::string expression = helperCallFor(node->children.front(), pointExpr, context);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = helperCallFor(node->children[i], pointExpr, context);
            expression = "max(" + expression + ", " + child + ")";
        }
        return expression;
    }
    case SdfNodeType::SmoothIntersect: {
        if (node->children.empty()) {
            result.errors.push_back("SmoothIntersect node has no children.");
            return "1e6";
        }
        result.usesSmoothMin = true;
        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        std::string expression = helperCallFor(node->children.front(), pointExpr, context);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = helperCallFor(node->children[i], pointExpr, context);
            expression = "(-sdf3d_smin(-(" + expression + "), -(" + child + "), " + glslFloat(smoothness) + "))";
        }
        return expression;
    }
    case SdfNodeType::Translate: {
        if (node->children.empty()) {
            result.errors.push_back("Translate node has no child.");
            return "1e6";
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Translate node ignores extra children.");
        }
        const float x = parameterOr(*node, "x", 0.0f);
        const float y = parameterOr(*node, "y", 0.0f);
        const float z = parameterOr(*node, "z", 0.0f);
        const std::string translate = node->stableId != 0
            ? glslNodeParam0(helperIdFor(node, context), glslVec4(x, y, z, 0.0f)) + ".xyz"
            : glslVec3(x, y, z);
        const std::string translatedPoint = "(" + pointExpr + " - " + translate + ")";
        return helperCallFor(node->children.front(), translatedPoint, context);
    }
    case SdfNodeType::Rotate: {
        if (node->children.empty()) {
            result.errors.push_back("Rotate node has no child.");
            return "1e6";
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Rotate node ignores extra children.");
        }
        result.usesRotate = true;
        const glm::vec4 fallback = rotationQuaternionForNode(*node);
        const std::string rotation = node->stableId != 0
            ? glslNodeParam0(helperIdFor(node, context), glslVec4(fallback.x, fallback.y, fallback.z, fallback.w))
            : glslVec4(fallback.x, fallback.y, fallback.z, fallback.w);
        const std::string rotatedPoint = "(transpose(sdf3d_rotationQuat(" + rotation + ")) * " + pointExpr + ")";
        return helperCallFor(node->children.front(), rotatedPoint, context);
    }
    case SdfNodeType::Scale: {
        if (node->children.empty()) {
            result.errors.push_back("Scale node has no child.");
            return "1e6";
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
            ? glslNodeParam0(helperIdFor(node, context), glslVec4(x, y, z, distanceScale))
            : glslVec4(x, y, z, distanceScale);
        const std::string scale = node->stableId != 0 ? "(" + scaleParam + ".xyz)" : glslVec3(x, y, z);
        const std::string scaledPoint = "(" + pointExpr + " / " + scale + ")";
        const std::string distanceScaleExpr = node->stableId != 0 ? scaleParam + ".w" : glslFloat(distanceScale);
        return "(" + helperCallFor(node->children.front(), scaledPoint, context) + " * " + distanceScaleExpr + ")";
    }
    case SdfNodeType::Repeat: {
        if (node->children.empty()) {
            result.errors.push_back("Repeat node has no child.");
            return "1e6";
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Repeat node ignores extra children.");
        }
        const float x = std::max(parameterOr(*node, "x", 2.0f), 0.0001f);
        const float y = std::max(parameterOr(*node, "y", 2.0f), 0.0001f);
        const float z = std::max(parameterOr(*node, "z", 2.0f), 0.0001f);
        const bool repeatX = parameterOr(*node, "repeatX", 1.0f) >= 0.5f;
        const bool repeatY = parameterOr(*node, "repeatY", 1.0f) >= 0.5f;
        const bool repeatZ = parameterOr(*node, "repeatZ", 1.0f) >= 0.5f;
        const std::string cell = glslVec3(x, y, z);
        std::string repeatedPoint = pointExpr;
        if (repeatX && repeatY && repeatZ) {
            repeatedPoint = "(mod(" + pointExpr + " + 0.5 * " + cell + ", " + cell + ") - 0.5 * " + cell + ")";
        } else if (repeatX || repeatY || repeatZ) {
            const std::string xSize = glslFloat(x);
            const std::string ySize = glslFloat(y);
            const std::string zSize = glslFloat(z);
            const std::string xExpr = repeatX ? "(mod(" + pointExpr + ".x + 0.5 * " + xSize + ", " + xSize + ") - 0.5 * " + xSize + ")" : pointExpr + ".x";
            const std::string yExpr = repeatY ? "(mod(" + pointExpr + ".y + 0.5 * " + ySize + ", " + ySize + ") - 0.5 * " + ySize + ")" : pointExpr + ".y";
            const std::string zExpr = repeatZ ? "(mod(" + pointExpr + ".z + 0.5 * " + zSize + ", " + zSize + ") - 0.5 * " + zSize + ")" : pointExpr + ".z";
            repeatedPoint = "vec3(" + xExpr + ", " + yExpr + ", " + zExpr + ")";
        }
        return helperCallFor(node->children.front(), repeatedPoint, context);
    }
    case SdfNodeType::Mirror: {
        if (node->children.empty()) {
            result.errors.push_back("Mirror node has no child.");
            return "1e6";
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
        return helperCallFor(node->children.front(), mirroredPoint, context);
    }
    case SdfNodeType::Twist: {
        if (node->children.empty()) {
            result.errors.push_back("Twist node has no child.");
            return "1e6";
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Twist node ignores extra children.");
        }
        const float strengthValue = parameterOr(*node, "strength", 1.0f);
        const std::string strength = glslFloat(strengthValue);
        const int axis = axisIndexFor(*node, 1.0f);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        std::string twistedPoint;
        if (axis == 0) {
            twistedPoint = "vec3(" + pointExpr + ".x, " + c + " * " + pointExpr + ".y - " + s + " * " + pointExpr + ".z, "
                + s + " * " + pointExpr + ".y + " + c + " * " + pointExpr + ".z)";
        } else if (axis == 1) {
            twistedPoint = "vec3(" + c + " * " + pointExpr + ".x - " + s + " * " + pointExpr + ".z, "
                + pointExpr + ".y, " + s + " * " + pointExpr + ".x + " + c + " * " + pointExpr + ".z)";
        } else {
            twistedPoint = "vec3(" + c + " * " + pointExpr + ".x - " + s + " * " + pointExpr + ".y, "
                + s + " * " + pointExpr + ".x + " + c + " * " + pointExpr + ".y, " + pointExpr + ".z)";
        }
        const std::string correction = "(1.0 + abs(" + glslFloat(strengthValue) + ") * " + glslFloat(warpCorrection) + ")";
        return "(" + helperCallFor(node->children.front(), twistedPoint, context) + " / " + correction + ")";
    }
    case SdfNodeType::Bend: {
        if (node->children.empty()) {
            result.errors.push_back("Bend node has no child.");
            return "1e6";
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Bend node ignores extra children.");
        }
        const float strengthValue = parameterOr(*node, "strength", 0.5f);
        const std::string strength = glslFloat(strengthValue);
        const int axis = axisIndexFor(*node, 0.0f);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        std::string bentPoint;
        if (axis == 0) {
            bentPoint = "vec3(" + pointExpr + ".x, " + c + " * " + pointExpr + ".y - " + s + " * " + pointExpr + ".z, "
                + s + " * " + pointExpr + ".y + " + c + " * " + pointExpr + ".z)";
        } else if (axis == 1) {
            bentPoint = "vec3(" + c + " * " + pointExpr + ".x - " + s + " * " + pointExpr + ".z, "
                + pointExpr + ".y, " + s + " * " + pointExpr + ".x + " + c + " * " + pointExpr + ".z)";
        } else {
            bentPoint = "vec3(" + c + " * " + pointExpr + ".x - " + s + " * " + pointExpr + ".y, "
                + s + " * " + pointExpr + ".x + " + c + " * " + pointExpr + ".y, " + pointExpr + ".z)";
        }
        const std::string correction = "(1.0 + abs(" + glslFloat(strengthValue) + ") * " + glslFloat(warpCorrection) + ")";
        return "(" + helperCallFor(node->children.front(), bentPoint, context) + " / " + correction + ")";
    }
    case SdfNodeType::MaterialOverride: {
        if (node->children.empty()) {
            result.errors.push_back("MaterialOverride node has no SDF input.");
            return "1e6";
        }
        if (node->children.size() > 1) {
            result.errors.push_back("MaterialOverride node ignores extra children.");
        }
        return helperCallFor(node->children.front(), pointExpr, context);
    }
    default:
        result.errors.push_back("Unsupported SDF node type in geometry helper emission: " + glslNodeTypeName(node->type));
        return "1e6";
    }
}

} // namespace sdf3d::glsl_emitter
