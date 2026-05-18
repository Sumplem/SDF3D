#pragma once

#include "sdf3d/scene/SdfNode.h"

#include <string>

namespace sdf3d::glsl_emitter {

float parameterOr(const SdfNode& node, const std::string& key, float fallback);
std::string glslFloat(float value);
std::string glslVec3(float x, float y, float z);
std::string glslVec4(float x, float y, float z, float w);
std::string glslNodeParam0(uint64_t nodeId, const std::string& fallback);
std::string glslHit(const std::string& distanceExpr, int materialId);
std::string glslNoHit();
std::string hitDistance(const std::string& hitExpr);

} // namespace sdf3d::glsl_emitter
