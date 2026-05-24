#include "GlslEmitterMath.h"

#include "GlslEmitterFormatting.h"
#include "sdf3d/scene/SdfRotationParams.h"

#include <algorithm>

namespace sdf3d::glsl_emitter {
namespace {

std::string strengthParamExpr(const SdfNode& node, GlslEmitMode mode, uint64_t nodeId, float fallback, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    const float strength = parameterOr(node, "strength", fallback);
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return glslFloat(strength);
    }
    return glslNodeParamComponent(mode, nodeId, glslVec4(strength, 0.0f, 0.0f, 0.0f), 'x', nodeParamSlotByNodeId);
}

std::string vec3RuntimeOrLiteral(GlslEmitMode mode, uint64_t nodeId, const std::string& fallbackVec4, const std::string& fallbackVec3, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return fallbackVec3;
    }

    return glslNodeParam0(mode, nodeId, fallbackVec4, nodeParamSlotByNodeId) + ".xyz";
}

std::string vec4RuntimeOrLiteral(GlslEmitMode mode, uint64_t nodeId, const std::string& fallbackVec4, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return fallbackVec4;
    }

    return glslNodeParam0(mode, nodeId, fallbackVec4, nodeParamSlotByNodeId);
}

std::string componentRuntimeOrLiteral(GlslEmitMode mode, uint64_t nodeId, const std::string& fallbackVec4, float fallback, char component, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return glslFloat(fallback);
    }

    return glslNodeParamComponent(mode, nodeId, fallbackVec4, component, nodeParamSlotByNodeId);
}

} // namespace

int axisIndexFor(const SdfNode& node, float defaultAxis)
{
    const int rawAxis = static_cast<int>(parameterOr(node, "axis", defaultAxis));
    return std::clamp(rawAxis, 0, 2);
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

std::string translatedPointFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::string& pointExpr, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    const float x = parameterOr(node, "x", 0.0f);
    const float y = parameterOr(node, "y", 0.0f);
    const float z = parameterOr(node, "z", 0.0f);
    const std::string translate = vec3RuntimeOrLiteral(mode, nodeId, glslVec4(x, y, z, 0.0f), glslVec3(x, y, z), nodeParamSlotByNodeId);
    return "(" + pointExpr + " - " + translate + ")";
}

std::string rotatedPointFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::string& pointExpr, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    const glm::vec4 fallback = rotationQuaternionForNode(node);
    const std::string rotation = vec4RuntimeOrLiteral(mode, nodeId, glslVec4(fallback.x, fallback.y, fallback.z, fallback.w), nodeParamSlotByNodeId);
    return "(transpose(sdf3d_rotationQuat(" + rotation + ")) * " + pointExpr + ")";
}

std::string scaledPointFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::string& pointExpr, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    const float uniformScale = parameterOr(node, "scale", 1.0f);
    const float x = std::max(parameterOr(node, "x", uniformScale), 0.0001f);
    const float y = std::max(parameterOr(node, "y", uniformScale), 0.0001f);
    const float z = std::max(parameterOr(node, "z", uniformScale), 0.0001f);
    const float distanceScale = std::min({x, y, z});
    const std::string scaleFallback = glslVec4(x, y, z, distanceScale);
    const std::string scale = vec3RuntimeOrLiteral(mode, nodeId, scaleFallback, glslVec3(x, y, z), nodeParamSlotByNodeId);
    return "(" + pointExpr + " / " + scale + ")";
}

std::string scaleDistanceFactorFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    const float uniformScale = parameterOr(node, "scale", 1.0f);
    const float x = std::max(parameterOr(node, "x", uniformScale), 0.0001f);
    const float y = std::max(parameterOr(node, "y", uniformScale), 0.0001f);
    const float z = std::max(parameterOr(node, "z", uniformScale), 0.0001f);
    const float distanceScale = std::min({x, y, z});
    return componentRuntimeOrLiteral(mode, nodeId, glslVec4(x, y, z, distanceScale), distanceScale, 'w', nodeParamSlotByNodeId);
}

std::string repeatedPointFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::string& pointExpr, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    const float x = std::max(parameterOr(node, "x", 2.0f), 0.0001f);
    const float y = std::max(parameterOr(node, "y", 2.0f), 0.0001f);
    const float z = std::max(parameterOr(node, "z", 2.0f), 0.0001f);
    const bool repeatX = parameterOr(node, "repeatX", 1.0f) != 0.0f;
    const bool repeatY = parameterOr(node, "repeatY", 1.0f) != 0.0f;
    const bool repeatZ = parameterOr(node, "repeatZ", 1.0f) != 0.0f;
    const std::string fallbackCell = glslVec3(x, y, z);
    const std::string cell = mode == GlslEmitMode::Baked || nodeId == 0
        ? fallbackCell
        : glslNodeParam0(mode, nodeId, glslVec4(x, y, z, 0.0f), nodeParamSlotByNodeId) + ".xyz";

    if (repeatX && repeatY && repeatZ) {
        return "(mod(" + pointExpr + " + 0.5 * " + cell + ", " + cell + ") - 0.5 * " + cell + ")";
    }
    if (!repeatX && !repeatY && !repeatZ) {
        return pointExpr;
    }

    const std::string xSize = mode == GlslEmitMode::Baked || nodeId == 0 ? glslFloat(x) : glslNodeParamComponent(mode, nodeId, glslVec4(x, y, z, 0.0f), 'x', nodeParamSlotByNodeId);
    const std::string ySize = mode == GlslEmitMode::Baked || nodeId == 0 ? glslFloat(y) : glslNodeParamComponent(mode, nodeId, glslVec4(x, y, z, 0.0f), 'y', nodeParamSlotByNodeId);
    const std::string zSize = mode == GlslEmitMode::Baked || nodeId == 0 ? glslFloat(z) : glslNodeParamComponent(mode, nodeId, glslVec4(x, y, z, 0.0f), 'z', nodeParamSlotByNodeId);
    const std::string xExpr = repeatX ? "(mod(" + pointExpr + ".x + 0.5 * " + xSize + ", " + xSize + ") - 0.5 * " + xSize + ")" : pointExpr + ".x";
    const std::string yExpr = repeatY ? "(mod(" + pointExpr + ".y + 0.5 * " + ySize + ", " + ySize + ") - 0.5 * " + ySize + ")" : pointExpr + ".y";
    const std::string zExpr = repeatZ ? "(mod(" + pointExpr + ".z + 0.5 * " + zSize + ", " + zSize + ") - 0.5 * " + zSize + ")" : pointExpr + ".z";
    return "vec3(" + xExpr + ", " + yExpr + ", " + zExpr + ")";
}

std::string mirroredPointFor(const SdfNode& node, const std::string& pointExpr)
{
    const bool mirrorX = parameterOr(node, "x", 1.0f) >= 0.5f;
    const bool mirrorY = parameterOr(node, "y", 0.0f) >= 0.5f;
    const bool mirrorZ = parameterOr(node, "z", 0.0f) >= 0.5f;
    const std::string xExpr = mirrorX ? "abs(" + pointExpr + ".x)" : pointExpr + ".x";
    const std::string yExpr = mirrorY ? "abs(" + pointExpr + ".y)" : pointExpr + ".y";
    const std::string zExpr = mirrorZ ? "abs(" + pointExpr + ".z)" : pointExpr + ".z";
    return "vec3(" + xExpr + ", " + yExpr + ", " + zExpr + ")";
}

DomainWarpExpr domainWarpFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::string& pointExpr, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    const float defaultStrength = node.type == SdfNodeType::Twist ? kTwistDefaultStrength : kBendDefaultStrength;
    const float defaultAxis = node.type == SdfNodeType::Twist ? kTwistDefaultAxis : kBendDefaultAxis;
    const std::string strength = strengthParamExpr(node, mode, nodeId, defaultStrength, nodeParamSlotByNodeId);
    const int axis = axisIndexFor(node, defaultAxis);
    const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
    const std::string angle = "(" + axisCoord + " * " + strength + ")";
    const std::string point = rotatePointAroundAxis(pointExpr, axis, "cos(" + angle + ")", "sin(" + angle + ")");
    return {point, warpCorrectionExpr(strength)};
}

std::string warpCorrectionExpr(const std::string& strengthExpr)
{
    return "(1.0 + abs(" + strengthExpr + ") * " + glslFloat(kWarpCorrection) + ")";
}

std::string glslRotationQuaternionFunction()
{
    return
        "mat3 sdf3d_rotationQuat(vec4 q)\n"
        "{\n"
        "    float x = q.x;\n"
        "    float y = q.y;\n"
        "    float z = q.z;\n"
        "    float w = q.w;\n"
        "    float xx = x * x;\n"
        "    float yy = y * y;\n"
        "    float zz = z * z;\n"
        "    float xy = x * y;\n"
        "    float xz = x * z;\n"
        "    float yz = y * z;\n"
        "    float wx = w * x;\n"
        "    float wy = w * y;\n"
        "    float wz = w * z;\n"
        "    return mat3(\n"
        "        1.0 - 2.0 * yy - 2.0 * zz, 2.0 * xy + 2.0 * wz, 2.0 * xz - 2.0 * wy,\n"
        "        2.0 * xy - 2.0 * wz, 1.0 - 2.0 * xx - 2.0 * zz, 2.0 * yz + 2.0 * wx,\n"
        "        2.0 * xz + 2.0 * wy, 2.0 * yz - 2.0 * wx, 1.0 - 2.0 * xx - 2.0 * yy);\n"
        "}\n\n";
}

} // namespace sdf3d::glsl_emitter
