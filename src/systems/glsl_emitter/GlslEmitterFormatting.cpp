#include "GlslEmitterFormatting.h"

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace sdf3d::glsl_emitter {

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

std::string glslVec4(float x, float y, float z, float w)
{
    return "vec4(" + glslFloat(x) + ", " + glslFloat(y) + ", " + glslFloat(z) + ", " + glslFloat(w) + ")";
}

std::string glslNodeParam0(GlslEmitMode mode, uint64_t nodeId, const std::string& fallback, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return fallback;
    }

    const auto slot = nodeParamSlotByNodeId.find(nodeId);
    if (slot == nodeParamSlotByNodeId.end()) {
        return fallback;
    }

    return "uNodeParams[" + std::to_string(slot->second) + "].data0";
}

std::string glslNodeParam1(GlslEmitMode mode, uint64_t nodeId, const std::string& fallback, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return fallback;
    }

    const auto slot = nodeParamSlotByNodeId.find(nodeId);
    if (slot == nodeParamSlotByNodeId.end()) {
        return fallback;
    }

    return "uNodeParams[" + std::to_string(slot->second) + "].data1";
}

std::string glslNodeParamComponent(GlslEmitMode mode, uint64_t nodeId, const std::string& fallback, char component, const std::unordered_map<uint64_t, uint32_t>& nodeParamSlotByNodeId)
{
    return glslNodeParam0(mode, nodeId, fallback, nodeParamSlotByNodeId) + "." + component;
}

} // namespace sdf3d::glsl_emitter
