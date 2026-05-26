#pragma once

#include "sdf3d/scene/SdfNode.h"
#include "sdf3d/systems/GlslEmitMode.h"

#include <string>
#include <unordered_map>

namespace sdf3d::glsl_emitter {

float parameterOr(const SdfNode& node, const std::string& key, float fallback);
std::string glslFloat(float value);
std::string glslVec3(float x, float y, float z);
std::string glslVec4(float x, float y, float z, float w);
std::string glslNodeParam0(GlslEmitMode mode, uint64_t nodeId, const std::string& fallback, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId);
std::string glslNodeParam1(GlslEmitMode mode, uint64_t nodeId, const std::string& fallback, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId);
std::string glslNodeParamComponent(GlslEmitMode mode, uint64_t nodeId, const std::string& fallback, char component, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId);

} // namespace sdf3d::glsl_emitter
