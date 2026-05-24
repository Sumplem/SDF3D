#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterInternal.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/systems/GlslNodeNames.h"

namespace sdf3d
{

    GlslEmitter::GlslEmitter(GlslEmitMode mode)
        : m_mode(mode)
    {
    }

} // namespace sdf3d

namespace sdf3d::glsl_emitter
{

    std::string emitGeometryExpression(const SdfNodePtr &node, const std::string &pointExpr, SdfCompileResult &result, SdfHelperEmitContext &context)
    {
        if (isSdfPrimitiveNode(node->type))
        {
            return emitPrimitiveGeometryExpression(node, pointExpr, result, context);
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
        if (node->type == SdfNodeType::Group)
        {
            if (node->children.empty()) {
                result.errors.push_back("Group node references a missing definition.");
                return "1e6";
            }
            return helperCallFor(node->children.front(), pointExpr, context);
        }

        result.errors.push_back("Unsupported SDF node type in geometry helper emission: " + glslNodeTypeName(node->type));
        return "1e6";
    }

} // namespace sdf3d::glsl_emitter
