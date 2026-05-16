#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "sdf3d/systems/GlslNodeNames.h"

#include <algorithm>

namespace sdf3d {

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
        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        std::string expression = emitNode(node->children.front(), pointExpr, result);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = emitNode(node->children[i], pointExpr, result);
            const std::string distance = "sdf3d_smin(" + hitDistance(expression) + ", " + hitDistance(child)
                + ", " + glslFloat(smoothness) + ")";
            // AGENT: Smooth blends keep nearer source material until shader
            // material blending exists, matching hard-min behavior at the edge.
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
        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        const std::string base = emitNode(node->children[0], pointExpr, result);
        const std::string cutter = emitNode(node->children[1], pointExpr, result);
        return "vec2((-sdf3d_smin(-(" + hitDistance(base) + "), " + hitDistance(cutter) + ", " + glslFloat(smoothness) + ")), " + base + ".y)";
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
        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        std::string expression = emitNode(node->children.front(), pointExpr, result);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = emitNode(node->children[i], pointExpr, result);
            const std::string distance = "(-sdf3d_smin(-(" + hitDistance(expression) + "), -(" + hitDistance(child)
                + "), " + glslFloat(smoothness) + "))";
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
