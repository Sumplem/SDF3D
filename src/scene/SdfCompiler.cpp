#include "sdf3d/scene/SdfCompiler.h"

#include "sdf3d/scene/SdfGraphCompiler.h"

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

int appendMaterial(SdfCompileResult& result, const SdfMaterial& material)
{
    const int id = static_cast<int>(result.materials.size());
    result.materials.push_back({material});
    return id;
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

std::string nodeTypeName(SdfNodeType type)
{
    switch (type) {
    case SdfNodeType::Sphere:
        return "Sphere";
    case SdfNodeType::Box:
        return "Box";
    case SdfNodeType::Cylinder:
        return "Cylinder";
    case SdfNodeType::Torus:
        return "Torus";
    case SdfNodeType::Plane:
        return "Plane";
    case SdfNodeType::Capsule:
        return "Capsule";
    case SdfNodeType::Cone:
        return "Cone";
    case SdfNodeType::RoundBox:
        return "RoundBox";
    case SdfNodeType::Union:
        return "Union";
    case SdfNodeType::SmoothUnion:
        return "SmoothUnion";
    case SdfNodeType::Subtract:
        return "Subtract";
    case SdfNodeType::SmoothSubtract:
        return "SmoothSubtract";
    case SdfNodeType::Intersect:
        return "Intersect";
    case SdfNodeType::SmoothIntersect:
        return "SmoothIntersect";
    case SdfNodeType::Translate:
        return "Translate";
    case SdfNodeType::Rotate:
        return "Rotate";
    case SdfNodeType::Scale:
        return "Scale";
    case SdfNodeType::Repeat:
        return "Repeat";
    case SdfNodeType::Mirror:
        return "Mirror";
    case SdfNodeType::Twist:
        return "Twist";
    case SdfNodeType::Bend:
        return "Bend";
    case SdfNodeType::MaterialOverride:
        return "MaterialOverride";
    case SdfNodeType::Output:
        return "Output";
    }

    return "Unknown";
}

} // namespace

SdfCompileResult SdfCompiler::compile(const SdfGraph& graph) const
{
    const SdfGraphLowerResult lowered = lowerSdfGraphToTree(graph);
    SdfCompileResult result = compile(lowered.root);
    result.errors.insert(result.errors.begin(), lowered.errors.begin(), lowered.errors.end());
    return result;
}

SdfCompileResult SdfCompiler::compile(const SdfNodePtr& root) const
{
    SdfCompileResult result;

    if (!root) {
        result.errors.push_back("Cannot compile an empty SDF tree.");
        // AGENT: Empty scenes still emit the material-aware entry point so the
        // runtime shader template stays valid before the user adds geometry.
        result.glsl =
            "vec2 sceneSDFWithMaterial(vec3 p)\n"
            "{\n"
            "    return vec2(1e6, 0.0);\n"
            "}\n\n"
            "float sceneSDF(vec3 p)\n"
            "{\n"
            "    return 1e6;\n"
            "}\n";
        return result;
    }

    const std::string expression = compileNode(root, "p", result);

    std::ostringstream glsl;
    if (result.usesBox) {
        glsl << "float sdf3d_box(vec3 p, vec3 b)\n";
        glsl << "{\n";
        glsl << "    vec3 q = abs(p) - b;\n";
        glsl << "    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);\n";
        glsl << "}\n\n";
    }

    if (result.usesCylinder) {
        glsl << "float sdf3d_cylinder(vec3 p, float radius, float halfHeight)\n";
        glsl << "{\n";
        glsl << "    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(radius, halfHeight);\n";
        glsl << "    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));\n";
        glsl << "}\n\n";
    }

    if (result.usesSmoothMin) {
        glsl << "float sdf3d_smin(float a, float b, float k)\n";
        glsl << "{\n";
        glsl << "    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);\n";
        glsl << "    return mix(b, a, h) - k * h * (1.0 - h);\n";
        glsl << "}\n\n";
    }

    if (result.usesRotate) {
        glsl << "mat3 sdf3d_rotationXYZ(vec3 degrees)\n";
        glsl << "{\n";
        glsl << "    vec3 r = radians(degrees);\n";
        glsl << "    vec3 c = cos(r);\n";
        glsl << "    vec3 s = sin(r);\n";
        glsl << "    mat3 rx = mat3(1.0, 0.0, 0.0, 0.0, c.x, s.x, 0.0, -s.x, c.x);\n";
        glsl << "    mat3 ry = mat3(c.y, 0.0, -s.y, 0.0, 1.0, 0.0, s.y, 0.0, c.y);\n";
        glsl << "    mat3 rz = mat3(c.z, s.z, 0.0, -s.z, c.z, 0.0, 0.0, 0.0, 1.0);\n";
        glsl << "    return rz * ry * rx;\n";
        glsl << "}\n\n";
    }

    glsl << "vec2 sceneSDFWithMaterial(vec3 p)\n";
    glsl << "{\n";
    glsl << "    return " << expression << ";\n";
    glsl << "}\n\n";

    glsl << "float sceneSDF(vec3 p)\n";
    glsl << "{\n";
    glsl << "    return sceneSDFWithMaterial(p).x;\n";
    glsl << "}\n";
    result.glsl = glsl.str();

    return result;
}

std::string SdfCompiler::compilePrimitiveNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    switch (node->type) {
    case SdfNodeType::Sphere: {
        const float radius = parameterOr(*node, "radius", 1.0f);
        // AGENT: Expressions are emitted inline for deterministic, compact GLSL
        // before introducing named temporaries for larger compiler passes.
        return glslHit("(length(" + pointExpr + ") - " + glslFloat(radius) + ")", appendMaterial(result, node->material));
    }

    case SdfNodeType::Box: {
        result.usesBox = true;
        const float x = parameterOr(*node, "x", 1.0f);
        const float y = parameterOr(*node, "y", 1.0f);
        const float z = parameterOr(*node, "z", 1.0f);
        return glslHit("sdf3d_box(" + pointExpr + ", " + glslVec3(x, y, z) + ")", appendMaterial(result, node->material));
    }

    case SdfNodeType::Cylinder: {
        result.usesCylinder = true;
        const float radius = parameterOr(*node, "radius", 1.0f);
        const float halfHeight = parameterOr(*node, "halfHeight", 1.0f);
        return glslHit("sdf3d_cylinder(" + pointExpr + ", " + glslFloat(radius) + ", " + glslFloat(halfHeight) + ")", appendMaterial(result, node->material));
    }

    case SdfNodeType::Torus: {
        const float majorRadius = parameterOr(*node, "majorRadius", 1.0f);
        const float minorRadius = parameterOr(*node, "minorRadius", 0.25f);
        return glslHit("(length(vec2(length(" + pointExpr + ".xz) - " + glslFloat(majorRadius) + ", "
                + pointExpr + ".y)) - " + glslFloat(minorRadius) + ")",
            appendMaterial(result, node->material));
    }

    case SdfNodeType::Plane: {
        const float normalX = parameterOr(*node, "normalX", 0.0f);
        const float normalY = parameterOr(*node, "normalY", 1.0f);
        const float normalZ = parameterOr(*node, "normalZ", 0.0f);
        const float offset = parameterOr(*node, "offset", 0.0f);
        return glslHit("(dot(" + pointExpr + ", normalize(" + glslVec3(normalX, normalY, normalZ) + ")) + " + glslFloat(offset) + ")",
            appendMaterial(result, node->material));
    }

    default:
        result.errors.push_back("Unsupported primitive node type in compiler: " + nodeTypeName(node->type));
        return glslNoHit();
    }
}

std::string SdfCompiler::compileBooleanNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    switch (node->type) {
    case SdfNodeType::Union: {
        if (node->children.empty()) {
            result.errors.push_back("Union node has no children.");
            return glslNoHit();
        }
        if (node->children.size() == 1) {
            return compileNode(node->children.front(), pointExpr, result);
        }

        std::string expression = compileNode(node->children.front(), pointExpr, result);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = compileNode(node->children[i], pointExpr, result);
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
            return compileNode(node->children.front(), pointExpr, result);
        }

        result.usesSmoothMin = true;
        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        std::string expression = compileNode(node->children.front(), pointExpr, result);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = compileNode(node->children[i], pointExpr, result);
            const std::string distance = "sdf3d_smin(" + hitDistance(expression) + ", " + hitDistance(child)
                + ", " + glslFloat(smoothness) + ")";
            // AGENT: Smooth blends keep the nearer source material until shader
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
            return compileNode(node->children.front(), pointExpr, result);
        }
        if (node->children.size() > 2) {
            result.errors.push_back("Subtract node ignores extra children beyond base and cutter.");
        }

        const std::string base = compileNode(node->children[0], pointExpr, result);
        const std::string cutter = compileNode(node->children[1], pointExpr, result);
        return "vec2(max(-(" + hitDistance(cutter) + "), " + hitDistance(base) + "), " + base + ".y)";
    }

    case SdfNodeType::SmoothSubtract: {
        if (node->children.empty()) {
            result.errors.push_back("SmoothSubtract node requires a base child.");
            return glslNoHit();
        }
        if (node->children.size() == 1) {
            result.errors.push_back("SmoothSubtract node is missing a cutter child; bypassing to base.");
            return compileNode(node->children.front(), pointExpr, result);
        }
        if (node->children.size() > 2) {
            result.errors.push_back("SmoothSubtract node ignores extra children beyond base and cutter.");
        }

        result.usesSmoothMin = true;
        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        const std::string base = compileNode(node->children[0], pointExpr, result);
        const std::string cutter = compileNode(node->children[1], pointExpr, result);
        return "vec2((-sdf3d_smin(-(" + hitDistance(base) + "), " + hitDistance(cutter) + ", " + glslFloat(smoothness) + ")), " + base + ".y)";
    }

    case SdfNodeType::Intersect: {
        if (node->children.empty()) {
            result.errors.push_back("Intersect node has no children.");
            return glslNoHit();
        }
        if (node->children.size() == 1) {
            return compileNode(node->children.front(), pointExpr, result);
        }

        std::string expression = compileNode(node->children.front(), pointExpr, result);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = compileNode(node->children[i], pointExpr, result);
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
            return compileNode(node->children.front(), pointExpr, result);
        }

        result.usesSmoothMin = true;
        const float smoothness = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
        std::string expression = compileNode(node->children.front(), pointExpr, result);
        for (size_t i = 1; i < node->children.size(); ++i) {
            const std::string child = compileNode(node->children[i], pointExpr, result);
            const std::string distance = "(-sdf3d_smin(-(" + hitDistance(expression) + "), -(" + hitDistance(child)
                + "), " + glslFloat(smoothness) + "))";
            expression = "(" + hitDistance(expression) + " > " + hitDistance(child) + " ? vec2(" + distance + ", "
                + expression + ".y) : vec2(" + distance + ", " + child + ".y))";
        }
        return expression;
    }

    default:
        result.errors.push_back("Unsupported boolean node type in compiler: " + nodeTypeName(node->type));
        return glslNoHit();
    }
}

std::string SdfCompiler::compileDomainNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
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
        return compileNode(node->children.front(), translatedPoint, result);
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
        // AGENT: Domain transforms apply the inverse transform to the sample
        // point; transpose is the inverse for an orthonormal rotation matrix.
        const std::string rotatedPoint = "(transpose(sdf3d_rotationXYZ(" + glslVec3(x, y, z) + ")) * " + pointExpr + ")";
        return compileNode(node->children.front(), rotatedPoint, result);
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
        const std::string child = compileNode(node->children.front(), scaledPoint, result);
        return "vec2(" + hitDistance(child) + " * " + glslFloat(scale) + ", " + child + ".y)";
    }

    default:
        result.errors.push_back("Unsupported domain node type in compiler: " + nodeTypeName(node->type));
        return glslNoHit();
    }
}

std::string SdfCompiler::compileNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
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
        return compilePrimitiveNode(node, pointExpr, result);

    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion:
    case SdfNodeType::Subtract:
    case SdfNodeType::SmoothSubtract:
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect:
        return compileBooleanNode(node, pointExpr, result);

    case SdfNodeType::Translate:
    case SdfNodeType::Rotate:
    case SdfNodeType::Scale:
        return compileDomainNode(node, pointExpr, result);

    default:
        result.errors.push_back("Unsupported SDF node type in compiler: " + nodeTypeName(node->type));
        return glslNoHit();
    }
}

} // namespace sdf3d
