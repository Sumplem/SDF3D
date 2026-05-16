#include "GlslEmitterSdfHelperContext.h"

#include "GlslEmitterFormatting.h"
#include "sdf3d/systems/GlslNodeNames.h"

#include <algorithm>

namespace sdf3d::glsl_emitter {

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
        const std::string translatedPoint = "(" + pointExpr + " - " + glslVec3(x, y, z) + ")";
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
        const float x = parameterOr(*node, "xDegrees", 0.0f);
        const float y = parameterOr(*node, "yDegrees", 0.0f);
        const float z = parameterOr(*node, "zDegrees", 0.0f);
        const std::string rotatedPoint = "(transpose(sdf3d_rotationXYZ(" + glslVec3(x, y, z) + ")) * " + pointExpr + ")";
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
        const float scale = std::max(parameterOr(*node, "scale", 1.0f), 0.0001f);
        const std::string scaledPoint = "(" + pointExpr + " / " + glslFloat(scale) + ")";
        return "(" + helperCallFor(node->children.front(), scaledPoint, context) + " * " + glslFloat(scale) + ")";
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
