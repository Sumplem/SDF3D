#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "GlslEmitterInternal.h"
#include "GlslEmitterMath.h"
#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/systems/GlslNodeNames.h"

#include <algorithm>

namespace sdf3d {
namespace {

std::string strengthParamExpr(const SdfNode& node, GlslEmitMode mode, uint64_t nodeId, float fallback)
{
    using namespace glsl_emitter;
    const float strength = parameterOr(node, "strength", fallback);
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return glslFloat(strength);
    }
    return glslNodeParamComponent(mode, nodeId, glslVec4(strength, 0.0f, 0.0f, 0.0f), 'x');
}

std::string vec3RuntimeOrLiteral(GlslEmitMode mode, uint64_t nodeId, const std::string& fallbackVec4, const std::string& fallbackVec3)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return fallbackVec3;
    }

    return glsl_emitter::glslNodeParam0(mode, nodeId, fallbackVec4) + ".xyz";
}

std::string vec4RuntimeOrLiteral(GlslEmitMode mode, uint64_t nodeId, const std::string& fallbackVec4)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return fallbackVec4;
    }

    return glsl_emitter::glslNodeParam0(mode, nodeId, fallbackVec4);
}

std::string componentRuntimeOrLiteral(GlslEmitMode mode, uint64_t nodeId, const std::string& fallbackVec4, float fallback, char component)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return glsl_emitter::glslFloat(fallback);
    }

    return glsl_emitter::glslNodeParamComponent(mode, nodeId, fallbackVec4, component);
}

} // namespace

std::string GlslEmitter::emitDomainNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    using namespace glsl_emitter;

    switch (node->type) {
    case SdfNodeType::Translate: {
        if (node->children.empty()) {
            result.errors.push_back("Translate node has no child.");
            return glslNoHit();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Translate node ignores extra children.");
        }

        const float x = parameterOr(*node, "x", 0.0f);
        const float y = parameterOr(*node, "y", 0.0f);
        const float z = parameterOr(*node, "z", 0.0f);
        const std::string translate = vec3RuntimeOrLiteral(m_mode, node->stableId, glslVec4(x, y, z, 0.0f), glslVec3(x, y, z));
        const std::string translatedPoint = "(" + pointExpr + " - " + translate + ")";
        return emitNode(node->children.front(), translatedPoint, result);
    }
    case SdfNodeType::Rotate: {
        if (node->children.empty()) {
            result.errors.push_back("Rotate node has no child.");
            return glslNoHit();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Rotate node ignores extra children.");
        }

        result.usesRotate = true;
        const glm::vec4 fallback = rotationQuaternionForNode(*node);
        const std::string rotation = vec4RuntimeOrLiteral(m_mode, node->stableId, glslVec4(fallback.x, fallback.y, fallback.z, fallback.w));
        const std::string rotatedPoint = "(transpose(sdf3d_rotationQuat(" + rotation + ")) * " + pointExpr + ")";
        return emitNode(node->children.front(), rotatedPoint, result);
    }
    case SdfNodeType::Scale: {
        if (node->children.empty()) {
            result.errors.push_back("Scale node has no child.");
            return glslNoHit();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Scale node ignores extra children.");
        }

        const float uniformScale = parameterOr(*node, "scale", 1.0f);
        const float x = std::max(parameterOr(*node, "x", uniformScale), 0.0001f);
        const float y = std::max(parameterOr(*node, "y", uniformScale), 0.0001f);
        const float z = std::max(parameterOr(*node, "z", uniformScale), 0.0001f);
        const float distanceScale = std::min({x, y, z});
        const std::string scaleFallback = glslVec4(x, y, z, distanceScale);
        const std::string scale = vec3RuntimeOrLiteral(m_mode, node->stableId, scaleFallback, glslVec3(x, y, z));
        const std::string scaledPoint = "(" + pointExpr + " / " + scale + ")";
        const std::string child = emitNode(node->children.front(), scaledPoint, result);
        const std::string distanceScaleExpr = componentRuntimeOrLiteral(m_mode, node->stableId, scaleFallback, distanceScale, 'w');
        return "vec2(" + hitDistance(child) + " * " + distanceScaleExpr + ", " + child + ".y)";
    }
    case SdfNodeType::Repeat: {
        if (node->children.empty()) {
            result.errors.push_back("Repeat node has no child.");
            return glslNoHit();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Repeat node ignores extra children.");
        }

        const std::string repeatedPoint = repeatedPointFor(*node, node->stableId, m_mode, pointExpr);
        return emitNode(node->children.front(), repeatedPoint, result);
    }
    case SdfNodeType::Mirror: {
        if (node->children.empty()) {
            result.errors.push_back("Mirror node has no child.");
            return glslNoHit();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Mirror node ignores extra children.");
        }

        const std::string mirroredPoint = mirroredPointFor(*node, pointExpr);
        return emitNode(node->children.front(), mirroredPoint, result);
    }
    case SdfNodeType::Twist: {
        if (node->children.empty()) {
            result.errors.push_back("Twist node has no child.");
            return glslNoHit();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Twist node ignores extra children.");
        }

        const std::string strength = strengthParamExpr(*node, m_mode, node->stableId, 1.0f);
        const int axis = axisIndexFor(*node, 1.0f);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        const std::string twistedPoint = rotatePointAroundAxis(pointExpr, axis, c, s);
        const std::string child = emitNode(node->children.front(), twistedPoint, result);
        const std::string correction = warpCorrectionExpr(strength);
        return "vec2(" + hitDistance(child) + " / " + correction + ", " + child + ".y)";
    }
    case SdfNodeType::Bend: {
        if (node->children.empty()) {
            result.errors.push_back("Bend node has no child.");
            return glslNoHit();
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Bend node ignores extra children.");
        }

        const std::string strength = strengthParamExpr(*node, m_mode, node->stableId, 0.5f);
        const int axis = axisIndexFor(*node, 0.0f);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        const std::string bentPoint = rotatePointAroundAxis(pointExpr, axis, c, s);
        const std::string child = emitNode(node->children.front(), bentPoint, result);
        const std::string correction = warpCorrectionExpr(strength);
        return "vec2(" + hitDistance(child) + " / " + correction + ", " + child + ".y)";
    }
    default:
        result.errors.push_back("Unsupported domain node type in compiler: " + glslNodeTypeName(node->type));
        return glslNoHit();
    }
}

} // namespace sdf3d

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
        const std::string translate = vec3RuntimeOrLiteral(context.mode, runtimeParamIdFor(node), glslVec4(x, y, z, 0.0f), glslVec3(x, y, z));
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
        const std::string rotation = vec4RuntimeOrLiteral(context.mode, runtimeParamIdFor(node), glslVec4(fallback.x, fallback.y, fallback.z, fallback.w));
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
        const std::string scaleFallback = glslVec4(x, y, z, distanceScale);
        const std::string scale = vec3RuntimeOrLiteral(context.mode, runtimeParamIdFor(node), scaleFallback, glslVec3(x, y, z));
        const std::string scaledPoint = "(" + pointExpr + " / " + scale + ")";
        const std::string distanceScaleExpr = componentRuntimeOrLiteral(context.mode, runtimeParamIdFor(node), scaleFallback, distanceScale, 'w');
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
        const std::string repeatedPoint = repeatedPointFor(*node, runtimeParamIdFor(node), context.mode, pointExpr);
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
        const std::string strength = strengthParamExpr(*node, context.mode, runtimeParamIdFor(node), defaultStrength);
        const int axis = axisIndexFor(*node, defaultAxis);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        const std::string warpedPoint = rotatePointAroundAxis(pointExpr, axis, c, s);
        const std::string correction = warpCorrectionExpr(strength);
        return "(" + helperCallFor(node->children.front(), warpedPoint, context) + " / " + correction + ")";
    }
    default:
        result.errors.push_back("Unsupported domain node type in geometry helper emission: " + glslNodeTypeName(node->type));
        return "1e6";
    }
}

} // namespace sdf3d::glsl_emitter
