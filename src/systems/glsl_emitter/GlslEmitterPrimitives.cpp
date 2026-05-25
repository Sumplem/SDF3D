#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "GlslEmitterInternal.h"
#include "sdf3d/systems/GlslNodeNames.h"

namespace sdf3d::glsl_emitter {
namespace {

std::string primitiveParamVec4(const SdfNode& node)
{
    using namespace glsl_emitter;

    switch (node.type) {
    case SdfNodeType::Sphere:
        return glslVec4(parameterOr(node, "radius", 1.0f), 0.0f, 0.0f, 0.0f);
    case SdfNodeType::Box:
        return glslVec4(parameterOr(node, "x", 1.0f), parameterOr(node, "y", 1.0f), parameterOr(node, "z", 1.0f), 0.0f);
    case SdfNodeType::Cylinder:
        return glslVec4(parameterOr(node, "radius", 1.0f), parameterOr(node, "halfHeight", 1.0f), 0.0f, 0.0f);
    case SdfNodeType::Torus:
        return glslVec4(parameterOr(node, "majorRadius", 1.0f), parameterOr(node, "minorRadius", 0.25f), 0.0f, 0.0f);
    case SdfNodeType::Plane:
        return glslVec4(
            parameterOr(node, "normalX", 0.0f),
            parameterOr(node, "normalY", 1.0f),
            parameterOr(node, "normalZ", 0.0f),
            parameterOr(node, "offset", 0.0f));
    case SdfNodeType::Capsule:
        return glslVec4(parameterOr(node, "radius", 0.35f), parameterOr(node, "halfHeight", 1.0f), 0.0f, 0.0f);
    case SdfNodeType::Cone:
        return glslVec4(parameterOr(node, "radius", 1.0f), parameterOr(node, "halfHeight", 1.0f), 0.0f, 0.0f);
    case SdfNodeType::RoundBox:
        return glslVec4(parameterOr(node, "x", 1.0f), parameterOr(node, "y", 1.0f), parameterOr(node, "z", 1.0f), parameterOr(node, "radius", 0.15f));
    default:
        return glslVec4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

std::string primitiveParamComponent(const SdfNode& node, const SdfHelperEmitContext& context, uint64_t nodeId, char component)
{
    if (context.mode == GlslEmitMode::Baked || nodeId == 0) {
        switch (node.type) {
        case SdfNodeType::Sphere:
            return glsl_emitter::glslFloat(glsl_emitter::parameterOr(node, "radius", 1.0f));
        case SdfNodeType::Cylinder:
            return glsl_emitter::glslFloat(component == 'x'
                ? glsl_emitter::parameterOr(node, "radius", 1.0f)
                : glsl_emitter::parameterOr(node, "halfHeight", 1.0f));
        case SdfNodeType::Torus:
            return glsl_emitter::glslFloat(component == 'x'
                ? glsl_emitter::parameterOr(node, "majorRadius", 1.0f)
                : glsl_emitter::parameterOr(node, "minorRadius", 0.25f));
        case SdfNodeType::Capsule:
            return glsl_emitter::glslFloat(component == 'x'
                ? glsl_emitter::parameterOr(node, "radius", 0.35f)
                : glsl_emitter::parameterOr(node, "halfHeight", 1.0f));
        case SdfNodeType::Cone:
            return glsl_emitter::glslFloat(component == 'x'
                ? glsl_emitter::parameterOr(node, "radius", 1.0f)
                : glsl_emitter::parameterOr(node, "halfHeight", 1.0f));
        case SdfNodeType::RoundBox:
            if (component == 'w') {
                return glsl_emitter::glslFloat(glsl_emitter::parameterOr(node, "radius", 0.15f));
            }
            break;
        default:
            break;
        }
    }

    return glsl_emitter::glslNodeParamComponent(context.mode, nodeId, primitiveParamVec4(node), component, context.nodeParamSlotByNodeId);
}

std::string primitiveVec3Param(const SdfNode& node, const SdfHelperEmitContext& context, uint64_t nodeId)
{
    if (context.mode == GlslEmitMode::Baked || nodeId == 0) {
        return glsl_emitter::glslVec3(
            glsl_emitter::parameterOr(node, "x", 1.0f),
            glsl_emitter::parameterOr(node, "y", 1.0f),
            glsl_emitter::parameterOr(node, "z", 1.0f));
    }

    return glsl_emitter::glslNodeParam0(context.mode, nodeId, primitiveParamVec4(node), context.nodeParamSlotByNodeId) + ".xyz";
}

std::string planeNormalParam(const SdfNode& node, const SdfHelperEmitContext& context, uint64_t nodeId)
{
    if (context.mode == GlslEmitMode::Baked || nodeId == 0) {
        return "normalize(" + glsl_emitter::glslVec3(
            glsl_emitter::parameterOr(node, "normalX", 0.0f),
            glsl_emitter::parameterOr(node, "normalY", 1.0f),
            glsl_emitter::parameterOr(node, "normalZ", 0.0f)) + ")";
    }

    return "normalize(" + glsl_emitter::glslNodeParam0(context.mode, nodeId, primitiveParamVec4(node), context.nodeParamSlotByNodeId) + ".xyz)";
}

std::string planeOffsetParam(const SdfNode& node, const SdfHelperEmitContext& context, uint64_t nodeId)
{
    if (context.mode == GlslEmitMode::Baked || nodeId == 0) {
        return glsl_emitter::glslFloat(glsl_emitter::parameterOr(node, "offset", 0.0f));
    }

    return glsl_emitter::glslNodeParamComponent(context.mode, nodeId, primitiveParamVec4(node), 'w', context.nodeParamSlotByNodeId);
}

} // namespace

std::string emitPrimitiveGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context)
{
    switch (node->type) {
    case SdfNodeType::Sphere: {
        const std::string radius = primitiveParamComponent(*node, context, runtimeParamIdFor(node), 'x');
        return "(length(" + pointExpr + ") - " + radius + ")";
    }
    case SdfNodeType::Box: {
        result.usesBox = true;
        const std::string box = primitiveVec3Param(*node, context, runtimeParamIdFor(node));
        return "sdf3d_box(" + pointExpr + ", " + box + ")";
    }
    case SdfNodeType::Cylinder: {
        result.usesCylinder = true;
        const std::string radius = primitiveParamComponent(*node, context, runtimeParamIdFor(node), 'x');
        const std::string halfHeight = primitiveParamComponent(*node, context, runtimeParamIdFor(node), 'y');
        return "sdf3d_cylinder(" + pointExpr + ", " + radius + ", " + halfHeight + ")";
    }
    case SdfNodeType::Torus: {
        const std::string majorRadius = primitiveParamComponent(*node, context, runtimeParamIdFor(node), 'x');
        const std::string minorRadius = primitiveParamComponent(*node, context, runtimeParamIdFor(node), 'y');
        return "(length(vec2(length(" + pointExpr + ".xz) - " + majorRadius + ", "
            + pointExpr + ".y)) - " + minorRadius + ")";
    }
    case SdfNodeType::Plane: {
        return "(dot(" + pointExpr + ", " + planeNormalParam(*node, context, runtimeParamIdFor(node)) + ") + "
            + planeOffsetParam(*node, context, runtimeParamIdFor(node)) + ")";
    }
    case SdfNodeType::Capsule: {
        const std::string radius = primitiveParamComponent(*node, context, runtimeParamIdFor(node), 'x');
        const std::string halfHeight = primitiveParamComponent(*node, context, runtimeParamIdFor(node), 'y');
        return "(length(vec3(" + pointExpr + ".x, " + pointExpr + ".y - clamp(" + pointExpr + ".y, -"
            + halfHeight + ", " + halfHeight + "), " + pointExpr + ".z)) - " + radius + ")";
    }
    case SdfNodeType::Cone: {
        result.usesCappedCone = true;
        const std::string radius = primitiveParamComponent(*node, context, runtimeParamIdFor(node), 'x');
        const std::string halfHeight = primitiveParamComponent(*node, context, runtimeParamIdFor(node), 'y');
        return "sdf3d_capped_cone(" + pointExpr + ", " + radius + ", " + halfHeight + ")";
    }
    case SdfNodeType::RoundBox: {
        result.usesBox = true;
        const std::string box = primitiveVec3Param(*node, context, runtimeParamIdFor(node));
        const std::string radius = primitiveParamComponent(*node, context, runtimeParamIdFor(node), 'w');
        return "(sdf3d_box(" + pointExpr + ", " + box + ") - " + radius + ")";
    }
    default:
        result.errors.push_back("Unsupported primitive node type in geometry helper emission: " + glslNodeTypeName(node->type));
        return "1e6";
    }
}

} // namespace sdf3d::glsl_emitter
