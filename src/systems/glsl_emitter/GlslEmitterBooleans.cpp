#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "GlslEmitterInternal.h"
#include "sdf3d/systems/GlslNodeNames.h"

#include <algorithm>

namespace sdf3d {
namespace {

std::string smoothnessExpr(const SdfNode& node, GlslEmitMode mode, uint64_t nodeId)
{
    using namespace glsl_emitter;

    const float smoothness = std::max(parameterOr(node, "smoothness", 0.25f), 0.0001f);
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return glslFloat(smoothness);
    }
    return "max(" + glslNodeParamComponent(mode, nodeId, glslVec4(smoothness, 0.0f, 0.0f, 0.0f), 'x') + ", 0.000100)";
}

} // namespace

std::string GlslEmitter::emitBooleanNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    using namespace glsl_emitter;

    switch (node->type) {
    case SdfNodeType::Union: {
        if (node->children.empty()) {
            result.errors.push_back("Union node has no children.");
            return glslNoHit();
        }
        if (node->children.size() == 1) {
            return emitNode(node->children.front(), pointExpr, result);
        }
        std::string expression = emitNode(node->children.front(), pointExpr, result);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = emitNode(node->children[i], pointExpr, result);
            expression = "vec2(min(" + hitDistance(expression) + ", " + hitDistance(child) + "), "
                + "(" + hitDistance(expression) + " < " + hitDistance(child) + " ? " + expression + ".y : " + child + ".y))";
        }
        return expression;
    }
    case SdfNodeType::SmoothUnion: {
        if (node->children.empty()) {
            result.errors.push_back("SmoothUnion node has no children.");
            return glslNoHit();
        }
        if (node->children.size() == 1) {
            return emitNode(node->children.front(), pointExpr, result);
        }
        result.usesSmoothMin = true;
        const std::string smoothness = smoothnessExpr(*node, m_mode, node->stableId);
        std::string expression = emitNode(node->children.front(), pointExpr, result);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = emitNode(node->children[i], pointExpr, result);
            const std::string distance = "sdf3d_smin(" + hitDistance(expression) + ", " + hitDistance(child)
                + ", " + smoothness + ")";
            expression = "(" + hitDistance(expression) + " < " + hitDistance(child) + " ? vec2(" + distance + ", "
                + expression + ".y) : vec2(" + distance + ", " + child + ".y))";
        }
        return expression;
    }
    case SdfNodeType::Subtract: {
        if (node->children.empty()) {
            result.errors.push_back("Subtract node requires a base child.");
            return glslNoHit();
        }
        if (node->children.size() == 1) {
            result.errors.push_back("Subtract node is missing a cutter child; bypassing to base.");
            return emitNode(node->children.front(), pointExpr, result);
        }
        if (node->children.size() > 2) {
            result.errors.push_back("Subtract node ignores extra children beyond base and cutter.");
        }
        const std::string base = emitNode(node->children[0], pointExpr, result);
        const std::string cutter = emitNode(node->children[1], pointExpr, result);
        return "vec2(max(-(" + hitDistance(cutter) + "), " + hitDistance(base) + "), " + base + ".y)";
    }
    case SdfNodeType::SmoothSubtract: {
        if (node->children.empty()) {
            result.errors.push_back("SmoothSubtract node requires a base child.");
            return glslNoHit();
        }
        if (node->children.size() == 1) {
            result.errors.push_back("SmoothSubtract node is missing a cutter child; bypassing to base.");
            return emitNode(node->children.front(), pointExpr, result);
        }
        if (node->children.size() > 2) {
            result.errors.push_back("SmoothSubtract node ignores extra children beyond base and cutter.");
        }
        result.usesSmoothMin = true;
        const std::string smoothness = smoothnessExpr(*node, m_mode, node->stableId);
        const std::string base = emitNode(node->children[0], pointExpr, result);
        const std::string cutter = emitNode(node->children[1], pointExpr, result);
        return "vec2((-sdf3d_smin(-(" + hitDistance(base) + "), " + hitDistance(cutter) + ", " + smoothness + ")), " + base + ".y)";
    }
    case SdfNodeType::Intersect: {
        if (node->children.empty()) {
            result.errors.push_back("Intersect node has no children.");
            return glslNoHit();
        }
        if (node->children.size() == 1) {
            return emitNode(node->children.front(), pointExpr, result);
        }
        std::string expression = emitNode(node->children.front(), pointExpr, result);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = emitNode(node->children[i], pointExpr, result);
            expression = "vec2(max(" + hitDistance(expression) + ", " + hitDistance(child) + "), "
                + "(" + hitDistance(expression) + " > " + hitDistance(child) + " ? " + expression + ".y : " + child + ".y))";
        }
        return expression;
    }
    case SdfNodeType::SmoothIntersect: {
        if (node->children.empty()) {
            result.errors.push_back("SmoothIntersect node has no children.");
            return glslNoHit();
        }
        if (node->children.size() == 1) {
            return emitNode(node->children.front(), pointExpr, result);
        }
        result.usesSmoothMin = true;
        const std::string smoothness = smoothnessExpr(*node, m_mode, node->stableId);
        std::string expression = emitNode(node->children.front(), pointExpr, result);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = emitNode(node->children[i], pointExpr, result);
            const std::string distance = "(-sdf3d_smin(-(" + hitDistance(expression) + "), -(" + hitDistance(child)
                + "), " + smoothness + "))";
            expression = "(" + hitDistance(expression) + " > " + hitDistance(child) + " ? vec2(" + distance + ", "
                + expression + ".y) : vec2(" + distance + ", " + child + ".y))";
        }
        return expression;
    }
    default:
        result.errors.push_back("Unsupported boolean node type in compiler: " + glslNodeTypeName(node->type));
        return glslNoHit();
    }
}

} // namespace sdf3d

namespace sdf3d::glsl_emitter {

std::string emitBooleanGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context)
{
    switch (node->type) {
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
        const std::string smoothness = smoothnessExpr(*node, context.mode, runtimeParamIdFor(node));
        std::string expression = helperCallFor(node->children.front(), pointExpr, context);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = helperCallFor(node->children[i], pointExpr, context);
            expression = "sdf3d_smin(" + expression + ", " + child + ", " + smoothness + ")";
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
        const std::string smoothness = smoothnessExpr(*node, context.mode, runtimeParamIdFor(node));
        const std::string base = helperCallFor(node->children[0], pointExpr, context);
        const std::string cutter = helperCallFor(node->children[1], pointExpr, context);
        return "(-sdf3d_smin(-(" + base + "), " + cutter + ", " + smoothness + "))";
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
        const std::string smoothness = smoothnessExpr(*node, context.mode, runtimeParamIdFor(node));
        std::string expression = helperCallFor(node->children.front(), pointExpr, context);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = helperCallFor(node->children[i], pointExpr, context);
            expression = "(-sdf3d_smin(-(" + expression + "), -(" + child + "), " + smoothness + "))";
        }
        return expression;
    }
    default:
        result.errors.push_back("Unsupported boolean node type in geometry helper emission: " + glslNodeTypeName(node->type));
        return "1e6";
    }
}

} // namespace sdf3d::glsl_emitter
