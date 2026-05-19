#include "GlslEmitterSdfHelperContext.h"

namespace sdf3d::glsl_emitter {

std::string emitMaterialGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context)
{
    if (node->children.empty()) {
        result.errors.push_back("MaterialOverride node has no SDF input.");
        return "1e6";
    }
    if (node->children.size() > 1) {
        result.errors.push_back("MaterialOverride node ignores extra children.");
    }
    return helperCallFor(node->children.front(), pointExpr, context);
}

} // namespace sdf3d::glsl_emitter
