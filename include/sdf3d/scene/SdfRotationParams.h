#pragma once

#include "sdf3d/scene/SdfNode.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <glm/glm.hpp>

namespace sdf3d {

inline constexpr const char* RotateParamQx = "qx";
inline constexpr const char* RotateParamQy = "qy";
inline constexpr const char* RotateParamQz = "qz";
inline constexpr const char* RotateParamQw = "qw";

inline float sdfRotationParameterOr(const SdfNode& node, const std::string& key, float fallback)
{
    const auto it = node.parameters.find(key);
    return it == node.parameters.end() ? fallback : it->second;
}

inline glm::vec4 normalizeRotationQuaternion(glm::vec4 q)
{
    const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (length <= 0.000001f) {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }
    return q / length;
}

inline glm::vec4 rotationAxisAngleQuaternion(int axis, float radians)
{
    const float half = radians * 0.5f;
    const float s = std::sin(half);
    if (axis == 0) {
        return {s, 0.0f, 0.0f, std::cos(half)};
    }
    if (axis == 1) {
        return {0.0f, s, 0.0f, std::cos(half)};
    }
    return {0.0f, 0.0f, s, std::cos(half)};
}

inline glm::vec4 multiplyRotationQuaternion(glm::vec4 a, glm::vec4 b)
{
    return normalizeRotationQuaternion({
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    });
}

inline glm::vec4 rotationQuaternionFromEulerDegrees(glm::vec3 degrees)
{
    constexpr float pi = 3.14159265358979323846f;
    const glm::vec3 radians = degrees * (pi / 180.0f);
    return multiplyRotationQuaternion(
        rotationAxisAngleQuaternion(2, radians.z),
        multiplyRotationQuaternion(rotationAxisAngleQuaternion(1, radians.y), rotationAxisAngleQuaternion(0, radians.x)));
}

inline glm::mat3 rotationMatrixFromQuaternion(glm::vec4 q)
{
    q = normalizeRotationQuaternion(q);
    const float x = q.x;
    const float y = q.y;
    const float z = q.z;
    const float w = q.w;
    return glm::mat3(
        {1.0f - 2.0f * y * y - 2.0f * z * z, 2.0f * x * y + 2.0f * w * z, 2.0f * x * z - 2.0f * w * y},
        {2.0f * x * y - 2.0f * w * z, 1.0f - 2.0f * x * x - 2.0f * z * z, 2.0f * y * z + 2.0f * w * x},
        {2.0f * x * z + 2.0f * w * y, 2.0f * y * z - 2.0f * w * x, 1.0f - 2.0f * x * x - 2.0f * y * y});
}

inline glm::vec3 rotationEulerDegreesFromQuaternion(glm::vec4 q)
{
    constexpr float pi = 3.14159265358979323846f;
    const glm::mat3 m = rotationMatrixFromQuaternion(q);
    const float y = std::asin(std::clamp(-m[0][2], -1.0f, 1.0f));
    const float cy = std::cos(y);
    float x = 0.0f;
    float z = 0.0f;
    if (std::abs(cy) > 0.0001f) {
        x = std::atan2(m[1][2], m[2][2]);
        z = std::atan2(m[0][1], m[0][0]);
    } else {
        x = std::atan2(-m[2][1], m[1][1]);
        z = 0.0f;
    }
    return {x * 180.0f / pi, y * 180.0f / pi, z * 180.0f / pi};
}

inline glm::vec4 rotationQuaternionForNode(const SdfNode& node)
{
    if (node.parameters.find(RotateParamQx) != node.parameters.end()
        || node.parameters.find(RotateParamQy) != node.parameters.end()
        || node.parameters.find(RotateParamQz) != node.parameters.end()
        || node.parameters.find(RotateParamQw) != node.parameters.end()) {
        return normalizeRotationQuaternion({
            sdfRotationParameterOr(node, RotateParamQx, 0.0f),
            sdfRotationParameterOr(node, RotateParamQy, 0.0f),
            sdfRotationParameterOr(node, RotateParamQz, 0.0f),
            sdfRotationParameterOr(node, RotateParamQw, 1.0f),
        });
    }
    return rotationQuaternionFromEulerDegrees({
        sdfRotationParameterOr(node, "xDegrees", 0.0f),
        sdfRotationParameterOr(node, "yDegrees", 0.0f),
        sdfRotationParameterOr(node, "zDegrees", 0.0f),
    });
}

inline void storeRotationQuaternion(SdfNode& node, glm::vec4 q)
{
    q = normalizeRotationQuaternion(q);
    node.parameters[RotateParamQx] = q.x;
    node.parameters[RotateParamQy] = q.y;
    node.parameters[RotateParamQz] = q.z;
    node.parameters[RotateParamQw] = q.w;
}

inline bool isHiddenRotationQuaternionParameter(const std::string& key)
{
    return key == RotateParamQx || key == RotateParamQy || key == RotateParamQz || key == RotateParamQw;
}

} // namespace sdf3d
