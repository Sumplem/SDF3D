#include "sdf3d/systems/GlslEmitter.h"

#include "sdf3d/systems/GlslNodeNames.h"
#include "sdf3d/systems/MaterialSystem.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

namespace sdf3d {
namespace {

float parameterOr(const SdfNode& node, const std::string& key, float fallback)
{
    const auto it = node.parameters.find(key);
    if (it == node.parameters.end()) {
        return fallback;
    }

    return it->second;
}

std::string glslFloat(float value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
}

std::string glslVec3(float x, float y, float z)
{
    return "vec3(" + glslFloat(x) + ", " + glslFloat(y) + ", " + glslFloat(z) + ")";
}

std::string glslHit(const std::string& distanceExpr, int materialId)
{
    return "vec2(" + distanceExpr + ", " + glslFloat(static_cast<float>(materialId)) + ")";
}

std::string glslNoHit()
{
    return "vec2(1e6, 0.0)";
}

std::string hitDistance(const std::string& hitExpr)
{
    return "(" + hitExpr + ").x";
}

} // namespace

std::string GlslEmitter::emitPrimitiveNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    const MaterialSystem materialSystem;
    const int defaultMaterialId = materialSystem.ensureDefaultMaterial(result);

    switch (node->type) {
    case SdfNodeType::Sphere: {
        const float radius = parameterOr(*node, "radius", 1.0f);
        // AGENT: Expressions stay inline for deterministic, compact GLSL
        // before introducing named temporaries for larger compiler passes.
        return glslHit("(length(" + pointExpr + ") - " + glslFloat(radius) + ")", defaultMaterialId);
    }

    case SdfNodeType::Box: {
        result.usesBox = true;
        const float x = parameterOr(*node, "x", 1.0f);
        const float y = parameterOr(*node, "y", 1.0f);
        const float z = parameterOr(*node, "z", 1.0f);
        return glslHit("sdf3d_box(" + pointExpr + ", " + glslVec3(x, y, z) + ")", defaultMaterialId);
    }

    case SdfNodeType::Cylinder: {
        result.usesCylinder = true;
        const float radius = parameterOr(*node, "radius", 1.0f);
        const float halfHeight = parameterOr(*node, "halfHeight", 1.0f);
        return glslHit("sdf3d_cylinder(" + pointExpr + ", " + glslFloat(radius) + ", " + glslFloat(halfHeight) + ")", defaultMaterialId);
    }

    case SdfNodeType::Torus: {
        const float majorRadius = parameterOr(*node, "majorRadius", 1.0f);
        const float minorRadius = parameterOr(*node, "minorRadius", 0.25f);
        return glslHit("(length(vec2(length(" + pointExpr + ".xz) - " + glslFloat(majorRadius) + ", "
                + pointExpr + ".y)) - " + glslFloat(minorRadius) + ")",
            defaultMaterialId);
    }

    case SdfNodeType::Plane: {
        const float normalX = parameterOr(*node, "normalX", 0.0f);
        const float normalY = parameterOr(*node, "normalY", 1.0f);
        const float normalZ = parameterOr(*node, "normalZ", 0.0f);
        const float offset = parameterOr(*node, "offset", 0.0f);
        return glslHit("(dot(" + pointExpr + ", normalize(" + glslVec3(normalX, normalY, normalZ) + ")) + " + glslFloat(offset) + ")",
            defaultMaterialId);
    }

    default:
        result.errors.push_back("Unsupported primitive node type in compiler: " + glslNodeTypeName(node->type));
        return glslNoHit();
    }
}

std::string GlslEmitter::emitMaterialNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    if (node->children.empty()) {
        result.errors.push_back("MaterialOverride node has no SDF input.");
        return glslNoHit();
    }
    if (node->children.size() > 1) {
        result.errors.push_back("MaterialOverride node ignores extra children.");
    }

    const MaterialSystem materialSystem;
    const int materialId = materialSystem.appendMaterial(result, node->material);
    const std::string child = emitNode(node->children.front(), pointExpr, result);
    return "vec2(" + hitDistance(child) + ", " + glslFloat(static_cast<float>(materialId)) + ")";
}

std::string GlslEmitter::emitBooleanNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
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

std::string GlslEmitter::emitDomainNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
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
        const std::string translatedPoint = "(" + pointExpr + " - " + glslVec3(x, y, z) + ")";
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
        const float x = parameterOr(*node, "xDegrees", 0.0f);
        const float y = parameterOr(*node, "yDegrees", 0.0f);
        const float z = parameterOr(*node, "zDegrees", 0.0f);
        // AGENT: Domain transforms apply inverse transform to sample point;
        // transpose is inverse for an orthonormal rotation matrix.
        const std::string rotatedPoint = "(transpose(sdf3d_rotationXYZ(" + glslVec3(x, y, z) + ")) * " + pointExpr + ")";
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

        const float scale = std::max(parameterOr(*node, "scale", 1.0f), 0.0001f);
        const std::string scaledPoint = "(" + pointExpr + " / " + glslFloat(scale) + ")";
        const std::string child = emitNode(node->children.front(), scaledPoint, result);
        return "vec2(" + hitDistance(child) + " * " + glslFloat(scale) + ", " + child + ".y)";
    }

    default:
        result.errors.push_back("Unsupported domain node type in compiler: " + glslNodeTypeName(node->type));
        return glslNoHit();
    }
}

std::string GlslEmitter::emitNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    if (!node) {
        result.errors.push_back("Encountered a null SDF node.");
        return glslNoHit();
    }

    switch (node->type) {
    case SdfNodeType::Sphere:
    case SdfNodeType::Box:
    case SdfNodeType::Cylinder:
    case SdfNodeType::Torus:
    case SdfNodeType::Plane:
        return emitPrimitiveNode(node, pointExpr, result);

    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion:
    case SdfNodeType::Subtract:
    case SdfNodeType::SmoothSubtract:
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect:
        return emitBooleanNode(node, pointExpr, result);

    case SdfNodeType::Translate:
    case SdfNodeType::Rotate:
    case SdfNodeType::Scale:
        return emitDomainNode(node, pointExpr, result);

    case SdfNodeType::MaterialOverride:
        return emitMaterialNode(node, pointExpr, result);

    default:
        result.errors.push_back("Unsupported SDF node type in compiler: " + glslNodeTypeName(node->type));
        return glslNoHit();
    }
}

} // namespace sdf3d
