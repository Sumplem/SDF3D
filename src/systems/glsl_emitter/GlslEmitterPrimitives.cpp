#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "GlslEmitterInternal.h"
#include "sdf3d/systems/GlslNodeNames.h"

#include <sstream>

namespace sdf3d::glsl_emitter {
namespace {

std::string primitiveParamVec4(const SdfNode& node)
{
    using namespace glsl_emitter;

    switch (node.type) {
    case SdfNodeType::Sphere:
    case SdfNodeType::SphereInstances:
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
        case SdfNodeType::SphereInstances:
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

std::string instanceParamVec4(const SdfNode& node)
{
    return glslVec4(
        parameterOr(node, "radius", 1.0f),
        0.0f,
        static_cast<float>(node.instancePositions.size()),
        0.0f);
}

std::string emitBakedSphereInstances(const SdfNode& node, const std::string& pointExpr)
{
    if (node.instancePositions.empty()) {
        return "1e6";
    }

    const std::string radius = glslFloat(parameterOr(node, "radius", 1.0f));
    std::string expression;
    for (std::size_t i = 0; i < node.instancePositions.size(); ++i) {
        const glm::vec3& position = node.instancePositions[i];
        const std::string distance = "(length((" + pointExpr + ") - " + glslVec3(position.x, position.y, position.z) + ") - " + radius + ")";
        expression = i == 0 ? distance : "min(" + expression + ", " + distance + ")";
    }
    return expression;
}

std::string instanceRangeVec4(const SdfNode& node)
{
    return glslVec4(0.0f, static_cast<float>(node.instancePositions.size()), 0.0f, 0.0f);
}

std::string instanceHelperName(SdfNodeType type)
{
    switch (type) {
    case SdfNodeType::Sphere:
        return "sdf3d_instance_sphere";
    case SdfNodeType::Box:
        return "sdf3d_instance_box";
    case SdfNodeType::Cylinder:
        return "sdf3d_instance_cylinder";
    case SdfNodeType::Torus:
        return "sdf3d_instance_torus";
    case SdfNodeType::Plane:
        return "sdf3d_instance_plane";
    case SdfNodeType::Capsule:
        return "sdf3d_instance_capsule";
    case SdfNodeType::Cone:
        return "sdf3d_instance_cone";
    case SdfNodeType::RoundBox:
        return "sdf3d_instance_round_box";
    default:
        return "";
    }
}

std::string bakedPrimitiveAt(const SdfNode& node, const std::string& pointExpr)
{
    switch (node.type) {
    case SdfNodeType::Sphere:
        return "(length(" + pointExpr + ") - " + glslFloat(parameterOr(node, "radius", 1.0f)) + ")";
    case SdfNodeType::Box:
        return "sdf3d_box(" + pointExpr + ", " + glslVec3(parameterOr(node, "x", 1.0f), parameterOr(node, "y", 1.0f), parameterOr(node, "z", 1.0f)) + ")";
    case SdfNodeType::Cylinder:
        return "sdf3d_cylinder(" + pointExpr + ", " + glslFloat(parameterOr(node, "radius", 1.0f)) + ", " + glslFloat(parameterOr(node, "halfHeight", 1.0f)) + ")";
    case SdfNodeType::Torus:
        return "(length(vec2(length(" + pointExpr + ".xz) - " + glslFloat(parameterOr(node, "majorRadius", 1.0f)) + ", "
            + pointExpr + ".y)) - " + glslFloat(parameterOr(node, "minorRadius", 0.25f)) + ")";
    case SdfNodeType::Plane:
        return "(dot(" + pointExpr + ", normalize(" + glslVec3(parameterOr(node, "normalX", 0.0f), parameterOr(node, "normalY", 1.0f), parameterOr(node, "normalZ", 0.0f)) + ")) + "
            + glslFloat(parameterOr(node, "offset", 0.0f)) + ")";
    case SdfNodeType::Capsule: {
        const std::string halfHeight = glslFloat(parameterOr(node, "halfHeight", 1.0f));
        return "(length(vec3(" + pointExpr + ".x, " + pointExpr + ".y - clamp(" + pointExpr + ".y, -" + halfHeight + ", " + halfHeight + "), " + pointExpr + ".z)) - "
            + glslFloat(parameterOr(node, "radius", 0.35f)) + ")";
    }
    case SdfNodeType::Cone:
        return "sdf3d_capped_cone(" + pointExpr + ", " + glslFloat(parameterOr(node, "radius", 1.0f)) + ", " + glslFloat(parameterOr(node, "halfHeight", 1.0f)) + ")";
    case SdfNodeType::RoundBox:
        return "(sdf3d_box(" + pointExpr + ", " + glslVec3(parameterOr(node, "x", 1.0f), parameterOr(node, "y", 1.0f), parameterOr(node, "z", 1.0f)) + ") - "
            + glslFloat(parameterOr(node, "radius", 0.15f)) + ")";
    default:
        return "1e6";
    }
}

std::string emitBakedPrimitiveInstances(const SdfNode& node, const std::string& pointExpr, SdfCompileResult& result)
{
    if (node.instancePositions.empty()) {
        return "1e6";
    }
    if (node.type == SdfNodeType::Box || node.type == SdfNodeType::RoundBox) {
        result.usesBox = true;
    }
    if (node.type == SdfNodeType::Cylinder) {
        result.usesCylinder = true;
    }
    if (node.type == SdfNodeType::Cone) {
        result.usesCappedCone = true;
    }

    std::string expression;
    for (std::size_t i = 0; i < node.instancePositions.size(); ++i) {
        const glm::vec3& position = node.instancePositions[i];
        const std::string localPoint = "((" + pointExpr + ") - " + glslVec3(position.x, position.y, position.z) + ")";
        const std::string distance = bakedPrimitiveAt(node, localPoint);
        expression = i == 0 ? distance : "min(" + expression + ", " + distance + ")";
    }
    return expression;
}

} // namespace

std::string emitPrimitiveGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context)
{
    if (node->type != SdfNodeType::SphereInstances && !node->instancePositions.empty()) {
        if (context.mode == GlslEmitMode::Baked) {
            return emitBakedPrimitiveInstances(*node, pointExpr, result);
        }

        const uint64_t nodeId = runtimeParamIdFor(node);
        const std::string params = glslNodeParam0(context.mode, nodeId, primitiveParamVec4(*node), context.nodeParamSlotByNodeId);
        const std::string range = glslNodeParam1(context.mode, nodeId, instanceRangeVec4(*node), context.nodeParamSlotByNodeId);
        const std::string helper = instanceHelperName(node->type);
        if (!helper.empty()) {
            return helper + "(" + pointExpr + ", " + params + ", " + range + ")";
        }
    }

    switch (node->type) {
    case SdfNodeType::Sphere: {
        const std::string radius = primitiveParamComponent(*node, context, runtimeParamIdFor(node), 'x');
        return "(length(" + pointExpr + ") - " + radius + ")";
    }
    case SdfNodeType::SphereInstances: {
        if (context.mode == GlslEmitMode::Baked) {
            return emitBakedSphereInstances(*node, pointExpr);
        }

        const std::string instanceParam = glslNodeParam0(context.mode, runtimeParamIdFor(node), instanceParamVec4(*node), context.nodeParamSlotByNodeId);
        std::ostringstream expression;
        expression << "sdf3d_sphere_instances(" << pointExpr << ", "
                   << instanceParam << ".x, int(" << instanceParam << ".y), int(" << instanceParam << ".z))";
        return expression.str();
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
