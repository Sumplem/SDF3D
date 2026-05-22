#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "GlslEmitterInternal.h"
#include "sdf3d/systems/GlslNodeNames.h"
#include "sdf3d/systems/MaterialSystem.h"

namespace sdf3d {
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
    default:
        return glslVec4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

std::string primitiveParamComponent(const SdfNode& node, GlslEmitMode mode, uint64_t nodeId, char component)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
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
        default:
            break;
        }
    }

    return glsl_emitter::glslNodeParamComponent(mode, nodeId, primitiveParamVec4(node), component);
}

std::string primitiveVec3Param(const SdfNode& node, GlslEmitMode mode, uint64_t nodeId)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return glsl_emitter::glslVec3(
            glsl_emitter::parameterOr(node, "x", 1.0f),
            glsl_emitter::parameterOr(node, "y", 1.0f),
            glsl_emitter::parameterOr(node, "z", 1.0f));
    }

    return glsl_emitter::glslNodeParam0(mode, nodeId, primitiveParamVec4(node)) + ".xyz";
}

std::string planeNormalParam(const SdfNode& node, GlslEmitMode mode, uint64_t nodeId)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return "normalize(" + glsl_emitter::glslVec3(
            glsl_emitter::parameterOr(node, "normalX", 0.0f),
            glsl_emitter::parameterOr(node, "normalY", 1.0f),
            glsl_emitter::parameterOr(node, "normalZ", 0.0f)) + ")";
    }

    return "normalize(" + glsl_emitter::glslNodeParam0(mode, nodeId, primitiveParamVec4(node)) + ".xyz)";
}

std::string planeOffsetParam(const SdfNode& node, GlslEmitMode mode, uint64_t nodeId)
{
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return glsl_emitter::glslFloat(glsl_emitter::parameterOr(node, "offset", 0.0f));
    }

    return glsl_emitter::glslNodeParamComponent(mode, nodeId, primitiveParamVec4(node), 'w');
}

} // namespace

std::string GlslEmitter::emitPrimitiveNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const
{
    using namespace glsl_emitter;

    const MaterialSystem materialSystem;
    const int defaultMaterialId = materialSystem.ensureDefaultMaterial(result);

    switch (node->type) {
    case SdfNodeType::Sphere: {
        const std::string radius = primitiveParamComponent(*node, m_mode, node->stableId, 'x');
        return glslHit("(length(" + pointExpr + ") - " + radius + ")", defaultMaterialId);
    }
    case SdfNodeType::Box: {
        result.usesBox = true;
        const std::string box = primitiveVec3Param(*node, m_mode, node->stableId);
        return glslHit("sdf3d_box(" + pointExpr + ", " + box + ")", defaultMaterialId);
    }
    case SdfNodeType::Cylinder: {
        result.usesCylinder = true;
        const std::string radius = primitiveParamComponent(*node, m_mode, node->stableId, 'x');
        const std::string halfHeight = primitiveParamComponent(*node, m_mode, node->stableId, 'y');
        return glslHit("sdf3d_cylinder(" + pointExpr + ", " + radius + ", " + halfHeight + ")", defaultMaterialId);
    }
    case SdfNodeType::Torus: {
        const std::string majorRadius = primitiveParamComponent(*node, m_mode, node->stableId, 'x');
        const std::string minorRadius = primitiveParamComponent(*node, m_mode, node->stableId, 'y');
        return glslHit("(length(vec2(length(" + pointExpr + ".xz) - " + majorRadius + ", "
                + pointExpr + ".y)) - " + minorRadius + ")",
            defaultMaterialId);
    }
    case SdfNodeType::Plane: {
        return glslHit("(dot(" + pointExpr + ", " + planeNormalParam(*node, m_mode, node->stableId) + ") + "
                + planeOffsetParam(*node, m_mode, node->stableId) + ")",
            defaultMaterialId);
    }
    default:
        result.errors.push_back("Unsupported primitive node type in compiler: " + glslNodeTypeName(node->type));
        return glslNoHit();
    }
}

} // namespace sdf3d

namespace sdf3d::glsl_emitter {

std::string emitPrimitiveGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context)
{
    switch (node->type) {
    case SdfNodeType::Sphere: {
        const std::string radius = primitiveParamComponent(*node, context.mode, runtimeParamIdFor(node), 'x');
        return "(length(" + pointExpr + ") - " + radius + ")";
    }
    case SdfNodeType::Box: {
        result.usesBox = true;
        const std::string box = primitiveVec3Param(*node, context.mode, runtimeParamIdFor(node));
        return "sdf3d_box(" + pointExpr + ", " + box + ")";
    }
    case SdfNodeType::Cylinder: {
        result.usesCylinder = true;
        const std::string radius = primitiveParamComponent(*node, context.mode, runtimeParamIdFor(node), 'x');
        const std::string halfHeight = primitiveParamComponent(*node, context.mode, runtimeParamIdFor(node), 'y');
        return "sdf3d_cylinder(" + pointExpr + ", " + radius + ", " + halfHeight + ")";
    }
    case SdfNodeType::Torus: {
        const std::string majorRadius = primitiveParamComponent(*node, context.mode, runtimeParamIdFor(node), 'x');
        const std::string minorRadius = primitiveParamComponent(*node, context.mode, runtimeParamIdFor(node), 'y');
        return "(length(vec2(length(" + pointExpr + ".xz) - " + majorRadius + ", "
            + pointExpr + ".y)) - " + minorRadius + ")";
    }
    case SdfNodeType::Plane: {
        return "(dot(" + pointExpr + ", " + planeNormalParam(*node, context.mode, runtimeParamIdFor(node)) + ") + "
            + planeOffsetParam(*node, context.mode, runtimeParamIdFor(node)) + ")";
    }
    default:
        result.errors.push_back("Unsupported primitive node type in geometry helper emission: " + glslNodeTypeName(node->type));
        return "1e6";
    }
}

} // namespace sdf3d::glsl_emitter
