#include "GlslEmitterSdfHelperContext.h"

#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/systems/GlslNodeNames.h"

namespace sdf3d::glsl_emitter {

uint64_t helperIdFor(const SdfNodePtr& node, SdfHelperEmitContext& context)
{
    if (node->stableId != 0) {
        return node->stableId;
    }

    const SdfNode* key = node.get();
    auto it = context.generatedIds.find(key);
    if (it != context.generatedIds.end()) {
        return it->second;
    }

    // AGENT: Legacy tree compiles have no graph IDs; generated IDs are isolated
    // above normal graph IDs so Step 1 remains usable before graph-only compile.
    const uint64_t id = context.nextGeneratedId++;
    context.generatedIds.emplace(key, id);
    return id;
}

std::string helperNameFor(const SdfNodePtr& node, SdfHelperEmitContext& context)
{
    return "sdf_node_" + std::to_string(helperIdFor(node, context));
}

std::string helperCallFor(const SdfNodePtr& node, const std::string& pointExpr, SdfHelperEmitContext& context)
{
    return helperNameFor(node, context) + "(" + pointExpr + ")";
}

std::string emitGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context)
{
    if (isSdfPrimitiveNode(node->type)) {
        return emitPrimitiveGeometryExpression(node, pointExpr, result);
    }
    if (isSdfBooleanNode(node->type)) {
        return emitBooleanGeometryExpression(node, pointExpr, result, context);
    }
    if (isSdfTransformNode(node->type)) {
        return emitDomainGeometryExpression(node, pointExpr, result, context);
    }
    if (node->type == SdfNodeType::MaterialOverride) {
        return emitMaterialGeometryExpression(node, pointExpr, result, context);
    }

    result.errors.push_back("Unsupported SDF node type in geometry helper emission: " + glslNodeTypeName(node->type));
    return "1e6";
}

} // namespace sdf3d::glsl_emitter
