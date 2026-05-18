#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/systems/GlslNodeNames.h"
#include "sdf3d/systems/MaterialSystem.h"

#include <algorithm>

namespace sdf3d {

namespace {

constexpr float warpCorrection = 1.5f;

int axisIndexFor(const SdfNode& node, float defaultAxis)
{
    const float rawAxis = glsl_emitter::parameterOr(node, "axis", defaultAxis);
    return static_cast<int>(std::clamp(rawAxis, 0.0f, 2.0f) + 0.5f);
}

std::string rotatePointAroundAxis(const std::string& pointExpr, int axis, const std::string& c, const std::string& s)
{
    if (axis == 0) {
        return "vec3(" + pointExpr + ".x, " + c + " * " + pointExpr + ".y - " + s + " * " + pointExpr + ".z, "
            + s + " * " + pointExpr + ".y + " + c + " * " + pointExpr + ".z)";
    }
    if (axis == 1) {
        return "vec3(" + c + " * " + pointExpr + ".x - " + s + " * " + pointExpr + ".z, "
            + pointExpr + ".y, " + s + " * " + pointExpr + ".x + " + c + " * " + pointExpr + ".z)";
    }
    return "vec3(" + c + " * " + pointExpr + ".x - " + s + " * " + pointExpr + ".y, "
        + s + " * " + pointExpr + ".x + " + c + " * " + pointExpr + ".y, " + pointExpr + ".z)";
}

std::string repeatedPointFor(const SdfNode& node, const std::string& pointExpr)
{
    const float x = std::max(glsl_emitter::parameterOr(node, "x", 2.0f), 0.0001f);
    const float y = std::max(glsl_emitter::parameterOr(node, "y", 2.0f), 0.0001f);
    const float z = std::max(glsl_emitter::parameterOr(node, "z", 2.0f), 0.0001f);
    const bool repeatX = glsl_emitter::parameterOr(node, "repeatX", 1.0f) >= 0.5f;
    const bool repeatY = glsl_emitter::parameterOr(node, "repeatY", 1.0f) >= 0.5f;
    const bool repeatZ = glsl_emitter::parameterOr(node, "repeatZ", 1.0f) >= 0.5f;
    const std::string cell = glsl_emitter::glslVec3(x, y, z);

    if (repeatX && repeatY && repeatZ) {
        return "(mod(" + pointExpr + " + 0.5 * " + cell + ", " + cell + ") - 0.5 * " + cell + ")";
    }
    if (!repeatX && !repeatY && !repeatZ) {
        return pointExpr;
    }

    const std::string xSize = glsl_emitter::glslFloat(x);
    const std::string ySize = glsl_emitter::glslFloat(y);
    const std::string zSize = glsl_emitter::glslFloat(z);
    const std::string xExpr = repeatX ? "(mod(" + pointExpr + ".x + 0.5 * " + xSize + ", " + xSize + ") - 0.5 * " + xSize + ")" : pointExpr + ".x";
    const std::string yExpr = repeatY ? "(mod(" + pointExpr + ".y + 0.5 * " + ySize + ", " + ySize + ") - 0.5 * " + ySize + ")" : pointExpr + ".y";
    const std::string zExpr = repeatZ ? "(mod(" + pointExpr + ".z + 0.5 * " + zSize + ", " + zSize + ") - 0.5 * " + zSize + ")" : pointExpr + ".z";
    return "vec3(" + xExpr + ", " + yExpr + ", " + zExpr + ")";
}

} // namespace

std::string GlslEmitter::emitPrimitiveNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    using namespace glsl_emitter;

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
    using namespace glsl_emitter;

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
        const std::string translate = node->stableId != 0
            ? glslNodeParam0(node->stableId, glslVec4(x, y, z, 0.0f)) + ".xyz"
            : glslVec3(x, y, z);
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
        // AGENT: Domain transforms apply inverse transform to sample point;
        // transpose is inverse for an orthonormal rotation matrix.
        const glm::vec4 fallback = rotationQuaternionForNode(*node);
        const std::string rotation = node->stableId != 0
            ? glslNodeParam0(node->stableId, glslVec4(fallback.x, fallback.y, fallback.z, fallback.w))
            : glslVec4(fallback.x, fallback.y, fallback.z, fallback.w);
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
        const std::string scaleParam = node->stableId != 0
            ? glslNodeParam0(node->stableId, glslVec4(x, y, z, distanceScale))
            : glslVec4(x, y, z, distanceScale);
        const std::string scale = node->stableId != 0 ? "(" + scaleParam + ".xyz)" : glslVec3(x, y, z);
        const std::string scaledPoint = "(" + pointExpr + " / " + scale + ")";
        const std::string child = emitNode(node->children.front(), scaledPoint, result);
        const std::string distanceScaleExpr = node->stableId != 0 ? scaleParam + ".w" : glslFloat(distanceScale);
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

        const std::string repeatedPoint = repeatedPointFor(*node, pointExpr);
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

        const bool mirrorX = parameterOr(*node, "x", 1.0f) >= 0.5f;
        const bool mirrorY = parameterOr(*node, "y", 0.0f) >= 0.5f;
        const bool mirrorZ = parameterOr(*node, "z", 0.0f) >= 0.5f;
        const std::string xExpr = mirrorX ? "abs(" + pointExpr + ".x)" : pointExpr + ".x";
        const std::string yExpr = mirrorY ? "abs(" + pointExpr + ".y)" : pointExpr + ".y";
        const std::string zExpr = mirrorZ ? "abs(" + pointExpr + ".z)" : pointExpr + ".z";
        const std::string mirroredPoint = "vec3(" + xExpr + ", " + yExpr + ", " + zExpr + ")";
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

        const float strengthValue = parameterOr(*node, "strength", 1.0f);
        const std::string strength = glslFloat(strengthValue);
        const int axis = axisIndexFor(*node, 1.0f);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        const std::string twistedPoint = rotatePointAroundAxis(pointExpr, axis, c, s);
        const std::string child = emitNode(node->children.front(), twistedPoint, result);
        const std::string correction = "(1.0 + abs(" + glslFloat(strengthValue) + ") * " + glslFloat(warpCorrection) + ")";
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

        const float strengthValue = parameterOr(*node, "strength", 0.5f);
        const std::string strength = glslFloat(strengthValue);
        const int axis = axisIndexFor(*node, 0.0f);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string c = "cos(" + angle + ")";
        const std::string s = "sin(" + angle + ")";
        const std::string bentPoint = rotatePointAroundAxis(pointExpr, axis, c, s);
        const std::string child = emitNode(node->children.front(), bentPoint, result);
        const std::string correction = "(1.0 + abs(" + glslFloat(strengthValue) + ") * " + glslFloat(warpCorrection) + ")";
        return "vec2(" + hitDistance(child) + " / " + correction + ", " + child + ".y)";
    }

    default:
        result.errors.push_back("Unsupported domain node type in compiler: " + glslNodeTypeName(node->type));
        return glslNoHit();
    }
}

std::string GlslEmitter::emitNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    using glsl_emitter::glslNoHit;

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
    case SdfNodeType::Repeat:
    case SdfNodeType::Mirror:
    case SdfNodeType::Twist:
    case SdfNodeType::Bend:
        return emitDomainNode(node, pointExpr, result);

    case SdfNodeType::MaterialOverride:
        return emitMaterialNode(node, pointExpr, result);

    default:
        result.errors.push_back("Unsupported SDF node type in compiler: " + glslNodeTypeName(node->type));
        return glslNoHit();
    }
}

} // namespace sdf3d
