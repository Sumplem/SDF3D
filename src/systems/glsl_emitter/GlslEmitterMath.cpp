#include "GlslEmitterMath.h"

#include "GlslEmitterFormatting.h"

#include <algorithm>

namespace sdf3d::glsl_emitter {

int axisIndexFor(const SdfNode& node, float defaultAxis)
{
    const float rawAxis = parameterOr(node, "axis", defaultAxis);
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
    const float x = std::max(parameterOr(node, "x", 2.0f), 0.0001f);
    const float y = std::max(parameterOr(node, "y", 2.0f), 0.0001f);
    const float z = std::max(parameterOr(node, "z", 2.0f), 0.0001f);
    const bool repeatX = parameterOr(node, "repeatX", 1.0f) >= 0.5f;
    const bool repeatY = parameterOr(node, "repeatY", 1.0f) >= 0.5f;
    const bool repeatZ = parameterOr(node, "repeatZ", 1.0f) >= 0.5f;
    const std::string cell = glslVec3(x, y, z);

    if (repeatX && repeatY && repeatZ) {
        return "(mod(" + pointExpr + " + 0.5 * " + cell + ", " + cell + ") - 0.5 * " + cell + ")";
    }
    if (!repeatX && !repeatY && !repeatZ) {
        return pointExpr;
    }

    const std::string xSize = glslFloat(x);
    const std::string ySize = glslFloat(y);
    const std::string zSize = glslFloat(z);
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

std::string warpCorrectionExpr(float strengthValue)
{
    return "(1.0 + abs(" + glslFloat(strengthValue) + ") * " + glslFloat(kWarpCorrection) + ")";
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
