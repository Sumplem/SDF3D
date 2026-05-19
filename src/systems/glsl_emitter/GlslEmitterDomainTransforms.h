#pragma once

#include "sdf3d/scene/SdfCompiler.h"

#include <string>

namespace sdf3d::glsl_emitter {

inline constexpr float kWarpCorrection = 1.5f;

int axisIndexFor(const SdfNode& node, float defaultAxis);
std::string rotatePointAroundAxis(const std::string& pointExpr, int axis, const std::string& c, const std::string& s);
std::string repeatedPointFor(const SdfNode& node, const std::string& pointExpr);
std::string mirroredPointFor(const SdfNode& node, const std::string& pointExpr);
std::string warpCorrectionExpr(float strengthValue);

} // namespace sdf3d::glsl_emitter
