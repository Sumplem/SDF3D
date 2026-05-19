#include "GlslEmitterSdfHelperContext.h"

#include "GlslEmitterDomainTransforms.h"
#include "GlslEmitterFormatting.h"
#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/systems/GlslNodeNames.h"

#include <algorithm>

namespace sdf3d::glsl_emitter {

std::string emitDomainGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context)
{
    switch (node->type) {
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
        const std::string repeatedPoint = repeatedPointFor(*node, pointExpr);
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
        const std::string mirroredPoint = mirroredPointFor(*node, pointExpr);
        return helperCallFor(node->children.front(), mirroredPoint, context);
    }
    case SdfNodeType::Twist:
    case SdfNodeType::Bend: {
        if (node->children.empty()) {
            result.errors.push_back(std::string(node->type == SdfNodeType::Twist ? "Twist" : "Bend") + " node has no child.");
            return "1e6";
        }
        if (node->children.size() > 1) {
            result.errors.push_back(std::string(node->type == SdfNodeType::Twist ? "Twist" : "Bend") + " node ignores extra children.");
        }
        const float defaultStrength = node->type == SdfNodeType::Twist ? 1.0f : 0.5f;
        const float defaultAxis = node->type == SdfNodeType::Twist ? 1.0f : 0.0f;
        const float strengthValue = parameterOr(*node, "strength", defaultStrength);
        const std::string strength = glslFloat(strengthValue);
        const int axis = axisIndexFor(*node, defaultAxis);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        const std::string warpedPoint = rotatePointAroundAxis(pointExpr, axis, c, s);
        const std::string correction = warpCorrectionExpr(strengthValue);
        return "(" + helperCallFor(node->children.front(), warpedPoint, context) + " / " + correction + ")";
    }
    default:
        result.errors.push_back("Unsupported domain node type in geometry helper emission: " + glslNodeTypeName(node->type));
        return "1e6";
    }
}

} // namespace sdf3d::glsl_emitter
