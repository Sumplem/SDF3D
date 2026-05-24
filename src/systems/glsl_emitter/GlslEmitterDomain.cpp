#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterInternal.h"
#include "GlslEmitterMath.h"
#include "sdf3d/systems/GlslNodeNames.h"

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
        const std::string translatedPoint = translatedPointFor(*node, runtimeParamIdFor(node), context.mode, pointExpr, context.nodeParamSlotByNodeId);
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
        const std::string rotatedPoint = rotatedPointFor(*node, runtimeParamIdFor(node), context.mode, pointExpr, context.nodeParamSlotByNodeId);
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
        const std::string scaledPoint = scaledPointFor(*node, runtimeParamIdFor(node), context.mode, pointExpr, context.nodeParamSlotByNodeId);
        const std::string distanceScaleExpr = scaleDistanceFactorFor(*node, runtimeParamIdFor(node), context.mode, context.nodeParamSlotByNodeId);
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
        const std::string repeatedPoint = repeatedPointFor(*node, runtimeParamIdFor(node), context.mode, pointExpr, context.nodeParamSlotByNodeId);
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
        const DomainWarpExpr warp = domainWarpFor(*node, runtimeParamIdFor(node), context.mode, pointExpr, context.nodeParamSlotByNodeId);
        return "(" + helperCallFor(node->children.front(), warp.point, context) + " / " + warp.correction + ")";
    }
    default:
        result.errors.push_back("Unsupported domain node type in geometry helper emission: " + glslNodeTypeName(node->type));
        return "1e6";
    }
}

} // namespace sdf3d::glsl_emitter
