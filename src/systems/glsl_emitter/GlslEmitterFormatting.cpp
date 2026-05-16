#include "GlslEmitterFormatting.h"

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

} // namespace sdf3d::glsl_emitter
