#pragma once

#include "sdf3d/scene/SdfCompiler.h"
#include "sdf3d/systems/GlslEmitMode.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace sdf3d::glsl_emitter {

inline constexpr float kWarpCorrection = 1.5f;
inline constexpr float kDomainAxisX = 0.0f;
inline constexpr float kDomainAxisY = 1.0f;
inline constexpr float kTwistDefaultStrength = 1.0f;
inline constexpr float kBendDefaultStrength = 0.5f;
inline constexpr float kTwistDefaultAxis = kDomainAxisY;
inline constexpr float kBendDefaultAxis = kDomainAxisX;

struct DomainWarpExpr {
    std::string point;
    std::string correction;
};

int axisIndexFor(const SdfNode& node, float defaultAxis);
std::string rotatePointAroundAxis(const std::string& pointExpr, int axis, const std::string& c, const std::string& s);
std::string translatedPointFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::string& pointExpr, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId);
std::string rotatedPointFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::string& pointExpr, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId);
std::string scaledPointFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::string& pointExpr, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId);
std::string scaleDistanceFactorFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId);
std::string repeatedPointFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::string& pointExpr, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId);
std::string mirroredPointFor(const SdfNode& node, const std::string& pointExpr);
DomainWarpExpr domainWarpFor(const SdfNode& node, uint64_t nodeId, GlslEmitMode mode, const std::string& pointExpr, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId);
std::string warpCorrectionExpr(const std::string& strengthExpr);
std::string glslRotationQuaternionFunction();

} // namespace sdf3d::glsl_emitter
