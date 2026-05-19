#include "GlslEmitterSdfHelperContext.h"

#include "GlslEmitterFormatting.h"
#include "sdf3d/systems/GlslNodeNames.h"

#include <algorithm>

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
    default:
        result.errors.push_back("Unsupported boolean node type in geometry helper emission: " + glslNodeTypeName(node->type));
        return "1e6";
    }
}

} // namespace sdf3d::glsl_emitter
