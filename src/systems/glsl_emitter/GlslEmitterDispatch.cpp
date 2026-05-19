#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "sdf3d/systems/GlslNodeNames.h"

namespace sdf3d {

std::string GlslEmitter::emitNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    using glsl_emitter::glslNoHit;

    if (!node) {
        result.errors.push_back("Encountered a null SDF node.");
        return glslNoHit();
    }

    switch (node->type) {
    case SdfNodeType::Sphere:
    case SdfNodeType::Box:
    case SdfNodeType::Cylinder:
    case SdfNodeType::Torus:
    case SdfNodeType::Plane:
        return emitPrimitiveNode(node, pointExpr, result);

    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion:
    case SdfNodeType::Subtract:
    case SdfNodeType::SmoothSubtract:
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect:
        return emitBooleanNode(node, pointExpr, result);

    case SdfNodeType::Translate:
    case SdfNodeType::Rotate:
    case SdfNodeType::Scale:
    case SdfNodeType::Repeat:
    case SdfNodeType::Mirror:
    case SdfNodeType::Twist:
    case SdfNodeType::Bend:
        return emitDomainNode(node, pointExpr, result);

    case SdfNodeType::MaterialOverride:
        return emitMaterialNode(node, pointExpr, result);

    default:
        result.errors.push_back("Unsupported SDF node type in compiler: " + glslNodeTypeName(node->type));
        return glslNoHit();
    }
}

} // namespace sdf3d
