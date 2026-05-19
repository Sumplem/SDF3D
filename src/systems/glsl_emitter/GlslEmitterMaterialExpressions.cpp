#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "sdf3d/systems/MaterialSystem.h"

namespace sdf3d {

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

} // namespace sdf3d
