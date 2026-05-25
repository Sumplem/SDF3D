#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterFormatting.h"
#include "GlslEmitterInternal.h"
#include "GlslEmitterMath.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/systems/GlslNodeNames.h"
#include "sdf3d/systems/MaterialSystem.h"

#include <algorithm>
#include <sstream>

namespace sdf3d::glsl_emitter {
namespace {

constexpr int kDefaultMaterialId = 0;

std::string sampleMaterialCall(int materialId, const std::string& pointExpr)
{
    return "sampleMaterial(" + std::to_string(materialId) + ", " + pointExpr + ")";
}

std::string materialGraphCall(const SdfNodePtr& node, const std::string& pointExpr, const SdfCompileResult& result)
{
    if (!node || node->materialId == 0) {
        return "";
    }
    const auto it = result.materialFunctionByRegistryId.find(node->materialId);
    if (it == result.materialFunctionByRegistryId.end()) {
        return "";
    }
    return it->second + "(" + pointExpr + ")";
}

std::string defaultMaterial(const std::string& pointExpr)
{
    return sampleMaterialCall(kDefaultMaterialId, pointExpr);
}

std::string helperDistanceFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers)
{
    const auto it = sdfHelpers.functionNameByNode.find(node.get());
    if (it == sdfHelpers.functionNameByNode.end()) {
        result.errors.push_back("Missing SDF helper for material evaluation.");
        return "1e6";
    }

    return it->second + "(" + pointExpr + ")";
}

std::string smoothnessExpr(const SdfNode& node, GlslEmitMode mode, uint64_t nodeId, const GlslSdfHelperBlock& sdfHelpers)
{
    const float smoothness = std::max(parameterOr(node, "smoothness", 0.25f), 0.0001f);
    if (mode == GlslEmitMode::Baked || nodeId == 0) {
        return glslFloat(smoothness);
    }
    return "max(" + glslNodeParamComponent(mode, nodeId, glslVec4(smoothness, 0.0f, 0.0f, 0.0f), 'x', sdfHelpers.nodeParamSlotByNodeId) + ", 0.000100)";
}

struct MaterialEval {
    std::string statements;
    std::string expression;
    std::string distance;
};

struct MaterialEvalContext {
    const MaterialSystem& materialSystem;
    int nextTemp = 0;
};

std::string nextTempName(MaterialEvalContext& context, const std::string& prefix)
{
    return "sdf3d_" + prefix + std::to_string(context.nextTemp++);
}

void appendStatements(std::ostringstream& statements, const MaterialEval& eval)
{
    statements << eval.statements;
}

std::string emitTemp(std::ostringstream& statements, MaterialEvalContext& context, const std::string& type, const std::string& prefix, const std::string& expression)
{
    const std::string name = nextTempName(context, prefix);
    statements << "    " << type << " " << name << " = " << expression << ";\n";
    return name;
}

MaterialEval emitMaterialEvalFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode,
    MaterialEvalContext& context);

MaterialEval emitBooleanMaterialEval(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode,
    MaterialEvalContext& context,
    bool useMax)
{
    if (node->children.empty()) {
        result.errors.push_back(glslNodeTypeName(node->type) + " node has no children.");
        return {"", defaultMaterial(pointExpr), "1e6"};
    }

    const bool smooth = node->type == SdfNodeType::SmoothUnion || node->type == SdfNodeType::SmoothIntersect;
    const std::string smoothness = smoothnessExpr(*node, mode, node->stableId, sdfHelpers);
    std::ostringstream statements;

    MaterialEval materialEval = emitMaterialEvalFor(node->children.front(), pointExpr, result, sdfHelpers, mode, context);
    appendStatements(statements, materialEval);
    std::string distance = emitTemp(statements, context, "float", "distance", materialEval.distance);
    std::string material = emitTemp(statements, context, "SdfMaterialSample", "material", materialEval.expression);

    for (size_t i = 1; i < node->children.size(); ++i) {
        MaterialEval childMaterialEval = emitMaterialEvalFor(node->children[i], pointExpr, result, sdfHelpers, mode, context);
        appendStatements(statements, childMaterialEval);
        const std::string childDistance = emitTemp(statements, context, "float", "distance", childMaterialEval.distance);
        const std::string childMaterial = emitTemp(statements, context, "SdfMaterialSample", "material", childMaterialEval.expression);
        if (smooth && !useMax) {
            const std::string blend = emitTemp(statements, context, "float", "blend", "clamp(0.5 + 0.5 * (" + childDistance + " - " + distance + ") / " + smoothness + ", 0.0, 1.0)");
            material = emitTemp(statements, context, "SdfMaterialSample", "material", "mixMaterial(" + childMaterial + ", " + material + ", " + blend + ")");
            distance = emitTemp(statements, context, "float", "distance", "sdf3d_smin(" + distance + ", " + childDistance + ", " + smoothness + ")");
        } else if (smooth) {
            const std::string blend = emitTemp(statements, context, "float", "blend", "clamp(0.5 + 0.5 * (" + distance + " - " + childDistance + ") / " + smoothness + ", 0.0, 1.0)");
            material = emitTemp(statements, context, "SdfMaterialSample", "material", "mixMaterial(" + childMaterial + ", " + material + ", " + blend + ")");
            distance = emitTemp(statements, context, "float", "distance", "(-sdf3d_smin(-(" + distance + "), -(" + childDistance + "), " + smoothness + "))");
        } else {
            const std::string chooseFirst = distance + (useMax ? " > " : " < ") + childDistance;
            material = emitTemp(statements, context, "SdfMaterialSample", "material", "selectMaterial(" + chooseFirst + ", " + material + ", " + childMaterial + ")");
            distance = emitTemp(statements, context, "float", "distance", std::string(useMax ? "max(" : "min(") + distance + ", " + childDistance + ")");
        }
    }

    return {statements.str(), material, distance};
}

} // namespace

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

namespace {

MaterialEval emitMaterialEvalFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode,
    MaterialEvalContext& context)
{
    if (!node) {
        result.errors.push_back("Encountered a null SDF node while emitting material evaluation.");
        return {"", defaultMaterial(pointExpr), "1e6"};
    }

    switch (node->type) {
    case SdfNodeType::Sphere:
    case SdfNodeType::Box:
    case SdfNodeType::Cylinder:
    case SdfNodeType::Torus:
    case SdfNodeType::Plane:
    case SdfNodeType::Capsule:
    case SdfNodeType::Cone:
    case SdfNodeType::RoundBox:
        return {"", defaultMaterial(pointExpr), helperDistanceFor(node, pointExpr, result, sdfHelpers)};

    case SdfNodeType::MaterialOverride: {
        if (node->children.empty()) {
            result.errors.push_back("MaterialOverride node has no SDF input.");
            return {"", defaultMaterial(pointExpr), "1e6"};
        }
        if (node->children.size() > 1) {
            result.errors.push_back("MaterialOverride node ignores extra children.");
        }

        const std::string graphCall = materialGraphCall(node, pointExpr, result);
        if (!graphCall.empty()) {
            return {"", graphCall, helperDistanceFor(node, pointExpr, result, sdfHelpers)};
        }
        const int materialId = context.materialSystem.appendMaterial(result, node->material);
        return {"", sampleMaterialCall(materialId, pointExpr), helperDistanceFor(node, pointExpr, result, sdfHelpers)};
    }
    case SdfNodeType::Group:
        if (node->children.empty()) {
            result.errors.push_back("Group node references a missing definition.");
            return {"", defaultMaterial(pointExpr), "1e6"};
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Group node ignores extra children.");
        }
        {
            MaterialEval child = emitMaterialEvalFor(node->children.front(), pointExpr, result, sdfHelpers, mode, context);
            child.distance = helperDistanceFor(node, pointExpr, result, sdfHelpers);
            return child;
        }
    case SdfNodeType::Translate: {
        if (node->children.empty()) {
            result.errors.push_back("Translate node has no child.");
            return {"", defaultMaterial(pointExpr), "1e6"};
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Translate node ignores extra children.");
        }

        const std::string translatedPoint = translatedPointFor(*node, node->stableId, mode, pointExpr, sdfHelpers.nodeParamSlotByNodeId);
        {
            MaterialEval child = emitMaterialEvalFor(node->children.front(), translatedPoint, result, sdfHelpers, mode, context);
            child.distance = helperDistanceFor(node, pointExpr, result, sdfHelpers);
            return child;
        }
    }
    case SdfNodeType::Rotate: {
        if (node->children.empty()) {
            result.errors.push_back("Rotate node has no child.");
            return {"", defaultMaterial(pointExpr), "1e6"};
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Rotate node ignores extra children.");
        }

        const std::string rotatedPoint = rotatedPointFor(*node, node->stableId, mode, pointExpr, sdfHelpers.nodeParamSlotByNodeId);
        {
            MaterialEval child = emitMaterialEvalFor(node->children.front(), rotatedPoint, result, sdfHelpers, mode, context);
            child.distance = helperDistanceFor(node, pointExpr, result, sdfHelpers);
            return child;
        }
    }
    case SdfNodeType::Scale: {
        if (node->children.empty()) {
            result.errors.push_back("Scale node has no child.");
            return {"", defaultMaterial(pointExpr), "1e6"};
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Scale node ignores extra children.");
        }

        const std::string scaledPoint = scaledPointFor(*node, node->stableId, mode, pointExpr, sdfHelpers.nodeParamSlotByNodeId);
        {
            MaterialEval child = emitMaterialEvalFor(node->children.front(), scaledPoint, result, sdfHelpers, mode, context);
            child.distance = helperDistanceFor(node, pointExpr, result, sdfHelpers);
            return child;
        }
    }
    case SdfNodeType::Repeat: {
        if (node->children.empty()) {
            result.errors.push_back("Repeat node has no child.");
            return {"", defaultMaterial(pointExpr), "1e6"};
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Repeat node ignores extra children.");
        }

        const std::string repeatedPoint = repeatedPointFor(*node, node->stableId, mode, pointExpr, sdfHelpers.nodeParamSlotByNodeId);
        return emitMaterialEvalFor(node->children.front(), repeatedPoint, result, sdfHelpers, mode, context);
    }
    case SdfNodeType::Mirror: {
        if (node->children.empty()) {
            result.errors.push_back("Mirror node has no child.");
            return {"", defaultMaterial(pointExpr), "1e6"};
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Mirror node ignores extra children.");
        }

        const std::string mirroredPoint = mirroredPointFor(*node, pointExpr);
        return emitMaterialEvalFor(node->children.front(), mirroredPoint, result, sdfHelpers, mode, context);
    }
    case SdfNodeType::Twist: {
        if (node->children.empty()) {
            result.errors.push_back("Twist node has no child.");
            return {"", defaultMaterial(pointExpr), "1e6"};
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Twist node ignores extra children.");
        }

        const DomainWarpExpr warp = domainWarpFor(*node, node->stableId, mode, pointExpr, sdfHelpers.nodeParamSlotByNodeId);
        {
            MaterialEval child = emitMaterialEvalFor(node->children.front(), warp.point, result, sdfHelpers, mode, context);
            child.distance = "(" + child.distance + " / " + warp.correction + ")";
            return child;
        }
    }
    case SdfNodeType::Bend: {
        if (node->children.empty()) {
            result.errors.push_back("Bend node has no child.");
            return {"", defaultMaterial(pointExpr), "1e6"};
        }
        if (node->children.size() > 1) {
            result.errors.push_back("Bend node ignores extra children.");
        }

        const DomainWarpExpr warp = domainWarpFor(*node, node->stableId, mode, pointExpr, sdfHelpers.nodeParamSlotByNodeId);
        {
            MaterialEval child = emitMaterialEvalFor(node->children.front(), warp.point, result, sdfHelpers, mode, context);
            child.distance = "(" + child.distance + " / " + warp.correction + ")";
            return child;
        }
    }
    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion:
        return emitBooleanMaterialEval(node, pointExpr, result, sdfHelpers, mode, context, false);
    case SdfNodeType::Subtract: {
        if (node->children.empty()) {
            result.errors.push_back("Subtract node requires a base child.");
            return {"", defaultMaterial(pointExpr), "1e6"};
        }
        if (node->children.size() == 1) {
            result.errors.push_back("Subtract node is missing a cutter child; bypassing to base.");
        }
        if (node->children.size() > 2) {
            result.errors.push_back("Subtract node ignores extra children beyond base and cutter.");
        }
        return emitMaterialEvalFor(node->children.front(), pointExpr, result, sdfHelpers, mode, context);
    }
    case SdfNodeType::SmoothSubtract: {
        if (node->children.empty()) {
            result.errors.push_back("SmoothSubtract node requires a base child.");
            return {"", defaultMaterial(pointExpr), "1e6"};
        }
        if (node->children.size() == 1) {
            result.errors.push_back("SmoothSubtract node is missing a cutter child; bypassing to base.");
            return emitMaterialEvalFor(node->children.front(), pointExpr, result, sdfHelpers, mode, context);
        }
        if (node->children.size() > 2) {
            result.errors.push_back("SmoothSubtract node ignores extra children beyond base and cutter.");
        }

        const std::string smoothness = smoothnessExpr(*node, mode, node->stableId, sdfHelpers);
        std::ostringstream statements;
        MaterialEval baseMaterialEval = emitMaterialEvalFor(node->children[0], pointExpr, result, sdfHelpers, mode, context);
        appendStatements(statements, baseMaterialEval);
        MaterialEval cutterMaterialEval = emitMaterialEvalFor(node->children[1], pointExpr, result, sdfHelpers, mode, context);
        appendStatements(statements, cutterMaterialEval);
        const std::string baseDistance = emitTemp(statements, context, "float", "distance", baseMaterialEval.distance);
        const std::string cutterDistance = emitTemp(statements, context, "float", "distance", cutterMaterialEval.distance);
        const std::string baseMaterial = emitTemp(statements, context, "SdfMaterialSample", "material", baseMaterialEval.expression);
        const std::string cutterMaterial = emitTemp(statements, context, "SdfMaterialSample", "material", cutterMaterialEval.expression);
        const std::string blend = emitTemp(statements, context, "float", "blend", "clamp(0.5 + 0.5 * (" + cutterDistance + " + " + baseDistance + ") / " + smoothness + ", 0.0, 1.0)");
        const std::string material = emitTemp(statements, context, "SdfMaterialSample", "material", "mixMaterial(" + cutterMaterial + ", " + baseMaterial + ", " + blend + ")");
        return {statements.str(), material, "(-sdf3d_smin(-(" + baseDistance + "), " + cutterDistance + ", " + smoothness + "))"};
    }
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect:
        return emitBooleanMaterialEval(node, pointExpr, result, sdfHelpers, mode, context, true);
    default:
        result.errors.push_back("Unsupported SDF node type in material evaluation: " + glslNodeTypeName(node->type));
        return {"", defaultMaterial(pointExpr), "1e6"};
    }
}

} // namespace

std::string emitMaterialBodyFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode,
    const MaterialSystem& materialSystem)
{
    MaterialEvalContext context{materialSystem};
    const MaterialEval eval = emitMaterialEvalFor(node, pointExpr, result, sdfHelpers, mode, context);

    std::ostringstream body;
    body << eval.statements;
    body << "    return " << eval.expression << ";\n";
    return body.str();
}

} // namespace sdf3d::glsl_emitter
