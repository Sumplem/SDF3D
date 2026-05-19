#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "GlslEmitterInternal.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/systems/GlslNodeNames.h"

namespace sdf3d
{

    std::string GlslEmitter::emitNode(const SdfNodePtr &node, const std::string &pointExpr, SdfCompileResult &result) const
    {
        using glsl_emitter::glslNoHit;

        if (!node)
        {
            result.errors.push_back("Encountered a null SDF node.");
            return glslNoHit();
        }

        if (isSdfPrimitiveNode(node->type))
        {
            return emitPrimitiveNode(node, pointExpr, result);
        }
        if (isSdfBooleanNode(node->type))
        {
            return emitBooleanNode(node, pointExpr, result);
        }
        if (isSdfTransformNode(node->type))
        {
            return emitDomainNode(node, pointExpr, result);
        }
        if (node->type == SdfNodeType::MaterialOverride)
        {
            return emitMaterialNode(node, pointExpr, result);
        }

        result.errors.push_back("Unsupported SDF node type in compiler: " + glslNodeTypeName(node->type));
        return glslNoHit();
    }

} // namespace sdf3d

namespace sdf3d::glsl_emitter
{

    std::string emitGeometryExpression(const SdfNodePtr &node, const std::string &pointExpr, SdfCompileResult &result, SdfHelperEmitContext &context)
    {
        if (isSdfPrimitiveNode(node->type))
        {
            return emitPrimitiveGeometryExpression(node, pointExpr, result);
        }
        if (isSdfBooleanNode(node->type))
        {
            return emitBooleanGeometryExpression(node, pointExpr, result, context);
        }
        if (isSdfTransformNode(node->type))
        {
            return emitDomainGeometryExpression(node, pointExpr, result, context);
        }
        if (node->type == SdfNodeType::MaterialOverride)
        {
            return emitMaterialGeometryExpression(node, pointExpr, result, context);
        }

        result.errors.push_back("Unsupported SDF node type in geometry helper emission: " + glslNodeTypeName(node->type));
        return "1e6";
    }

} // namespace sdf3d::glsl_emitter
