#include "sdf3d/systems/GlslEmitter.h"

#include "GlslEmitterInternal.h"
#include "GlslEmitterFormatting.h"
#include "GlslEmitterMath.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/systems/MaterialSystem.h"
#include "sdf3d/systems/GlslNodeNames.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace sdf3d::glsl_emitter {
namespace {

constexpr uint64_t kMaxSceneNodeSwitchId = 2147483647u;

uint64_t generatedHelperIdFor(const SdfNodePtr& node, SdfHelperEmitContext& context)
{
    const SdfNode* key = node.get();
    auto it = context.generatedIds.find(key);
    if (it != context.generatedIds.end()) {
        return it->second;
    }

    // AGENT: Generated helper IDs stay int-safe for sceneNodeSDF switch labels;
    // node->stableId may still be a wider runtime-param key.
    const uint64_t id = context.nextGeneratedId++;
    context.generatedIds.emplace(key, id);
    return id;
}

} // namespace

uint64_t helperIdFor(const SdfNodePtr& node, SdfHelperEmitContext& context)
{
    if (node->stableId != 0 && node->stableId <= kMaxSceneNodeSwitchId) {
        return node->stableId;
    }

    return generatedHelperIdFor(node, context);
}

uint64_t runtimeParamIdFor(const SdfNodePtr& node)
{
    return node ? node->stableId : 0;
}

std::string helperNameFor(const SdfNodePtr& node, SdfHelperEmitContext& context)
{
    return "sdf_node_" + std::to_string(helperIdFor(node, context));
}

std::string helperCallFor(const SdfNodePtr& node, const std::string& pointExpr, SdfHelperEmitContext& context)
{
    return helperNameFor(node, context) + "(" + pointExpr + ")";
}

} // namespace sdf3d::glsl_emitter

namespace sdf3d {
namespace {

constexpr uint64_t kGeneratedHelperIdBase = 1000000;

std::string helperDistanceFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers)
{
    const auto it = sdfHelpers.functionNameByNode.find(node.get());
    if (it == sdfHelpers.functionNameByNode.end()) {
        result.errors.push_back("Missing SDF helper for pick-id evaluation.");
        return "1e6";
    }

    return it->second + "(" + pointExpr + ")";
}

uint64_t helperNodeIdFor(
    const SdfNodePtr& node,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers)
{
    const auto it = sdfHelpers.nodeIdByNode.find(node.get());
    if (it == sdfHelpers.nodeIdByNode.end()) {
        result.errors.push_back("Missing SDF helper id for pick-id evaluation.");
        return 0;
    }

    return it->second;
}

std::string emitPickIdFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode);

std::string nodePickIdLiteral(
    const SdfNodePtr& node,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers)
{
    return std::to_string(static_cast<int>(helperNodeIdFor(node, result, sdfHelpers)));
}

std::string emitDomainPickIdFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode)
{
    using namespace glsl_emitter;

    if (node->children.empty()) {
        result.errors.push_back(glslNodeTypeName(node->type) + " node has no child.");
        return "-1";
    }
    if (node->children.size() > 1) {
        result.errors.push_back(glslNodeTypeName(node->type) + " node ignores extra children.");
    }

    switch (node->type) {
    case SdfNodeType::Translate: {
        // AGENT: Pick IDs identify the visible transform wrapper so shader
        // highlight can gate tint by the winning object instead of distance.
        return nodePickIdLiteral(node, result, sdfHelpers);
    }
    case SdfNodeType::Rotate: {
        return nodePickIdLiteral(node, result, sdfHelpers);
    }
    case SdfNodeType::Scale: {
        return nodePickIdLiteral(node, result, sdfHelpers);
    }
    case SdfNodeType::Repeat:
        return emitPickIdFor(node->children.front(), repeatedPointFor(*node, node->stableId, mode, pointExpr), result, sdfHelpers, mode);
    case SdfNodeType::Mirror:
        return emitPickIdFor(node->children.front(), mirroredPointFor(*node, pointExpr), result, sdfHelpers, mode);
    case SdfNodeType::Twist:
    case SdfNodeType::Bend: {
        const float defaultStrength = node->type == SdfNodeType::Twist ? 1.0f : 0.5f;
        const float defaultAxis = node->type == SdfNodeType::Twist ? 1.0f : 0.0f;
        const float strengthValue = parameterOr(*node, "strength", defaultStrength);
        const std::string strength = glslNodeParamComponent(mode, node->stableId, glslVec4(strengthValue, 0.0f, 0.0f, 0.0f), 'x');
        const int axis = axisIndexFor(*node, defaultAxis);
        const std::string axisCoord = axis == 0 ? pointExpr + ".x" : (axis == 1 ? pointExpr + ".y" : pointExpr + ".z");
        const std::string angle = "(" + axisCoord + " * " + strength + ")";
        const std::string warpedPoint = rotatePointAroundAxis(pointExpr, axis, "cos(" + angle + ")", "sin(" + angle + ")");
        return emitPickIdFor(node->children.front(), warpedPoint, result, sdfHelpers, mode);
    }
    default:
        break;
    }

    return "-1";
}

std::string emitBooleanPickIdFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode)
{
    using namespace glsl_emitter;

    if (node->children.empty()) {
        result.errors.push_back(glslNodeTypeName(node->type) + " node has no children.");
        return "-1";
    }
    if (node->children.size() == 1) {
        return emitPickIdFor(node->children.front(), pointExpr, result, sdfHelpers, mode);
    }
    if (node->type == SdfNodeType::Subtract || node->type == SdfNodeType::SmoothSubtract) {
        return emitPickIdFor(node->children.front(), pointExpr, result, sdfHelpers, mode);
    }

    const bool useMax = node->type == SdfNodeType::Intersect || node->type == SdfNodeType::SmoothIntersect;
    const bool smoothUnion = node->type == SdfNodeType::SmoothUnion;
    const bool smoothIntersect = node->type == SdfNodeType::SmoothIntersect;
    const float smoothnessValue = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
    const std::string smoothness = "max(" + glslNodeParamComponent(mode, node->stableId, glslVec4(smoothnessValue, 0.0f, 0.0f, 0.0f), 'x') + ", 0.000100)";
    std::string distance = helperDistanceFor(node->children.front(), pointExpr, result, sdfHelpers);
    std::string pickId = emitPickIdFor(node->children.front(), pointExpr, result, sdfHelpers, mode);
    for (std::size_t i = 1; i < node->children.size(); ++i) {
        const std::string childDistance = helperDistanceFor(node->children[i], pointExpr, result, sdfHelpers);
        const std::string childPickId = emitPickIdFor(node->children[i], pointExpr, result, sdfHelpers, mode);
        const std::string chooseFirst = "(" + distance + (useMax ? " > " : " < ") + childDistance + ")";
        pickId = "(" + chooseFirst + " ? " + pickId + " : " + childPickId + ")";
        if (smoothUnion) {
            distance = "sdf3d_smin(" + distance + ", " + childDistance + ", " + smoothness + ")";
        } else if (smoothIntersect) {
            distance = "(-sdf3d_smin(-(" + distance + "), -(" + childDistance + "), " + smoothness + "))";
        } else {
            distance = std::string(useMax ? "max(" : "min(") + distance + ", " + childDistance + ")";
        }
    }
    return pickId;
}

std::string emitPickIdFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode)
{
    if (!node) {
        result.errors.push_back("Encountered a null SDF node while emitting pick-id evaluation.");
        return "-1";
    }
    if (isSdfPrimitiveNode(node->type)) {
        return nodePickIdLiteral(node, result, sdfHelpers);
    }
    if (isSdfBooleanNode(node->type)) {
        return emitBooleanPickIdFor(node, pointExpr, result, sdfHelpers, mode);
    }
    if (isSdfTransformNode(node->type)) {
        return emitDomainPickIdFor(node, pointExpr, result, sdfHelpers, mode);
    }
    if (node->type == SdfNodeType::MaterialOverride) {
        if (node->children.empty()) {
            result.errors.push_back("MaterialOverride node has no SDF input.");
            return "-1";
        }
        return emitPickIdFor(node->children.front(), pointExpr, result, sdfHelpers, mode);
    }
    if (node->type == SdfNodeType::Group) {
        if (node->children.empty()) {
            result.errors.push_back("Group node references a missing definition.");
            return "-1";
        }
        return nodePickIdLiteral(node, result, sdfHelpers);
    }

    result.errors.push_back("Unsupported SDF node type in pick-id evaluation: " + glslNodeTypeName(node->type));
    return "-1";
}

void emitSdfHelperPostorder(
    const SdfNodePtr& node,
    SdfCompileResult& result,
    GlslSdfHelperBlock& block,
    glsl_emitter::SdfHelperEmitContext& context,
    std::unordered_set<uint64_t>& emittedIds,
    std::unordered_set<const SdfNode*>& visiting)
{
    if (!node) {
        result.errors.push_back("Encountered a null SDF node.");
        return;
    }

    if (visiting.find(node.get()) != visiting.end()) {
        result.errors.push_back("Cycle detected while emitting SDF helpers.");
        return;
    }

    const uint64_t id = glsl_emitter::helperIdFor(node, context);
    if (emittedIds.find(id) != emittedIds.end()) {
        return;
    }

    visiting.insert(node.get());
    for (const SdfNodePtr& child : node->children) {
        if (child && isSdfMaterialNode(child->type)) {
            continue;
        }
        emitSdfHelperPostorder(child, result, block, context, emittedIds, visiting);
    }
    visiting.erase(node.get());

    const std::string functionName = "sdf_node_" + std::to_string(id);
    const std::string expression = glsl_emitter::emitGeometryExpression(node, "p", result, context);

    std::ostringstream glsl;
    glsl << "float " << functionName << "(vec3 p)\n";
    glsl << "{\n";
    glsl << "    return " << expression << ";\n";
    glsl << "}\n";

    block.helpers.push_back({id, functionName, glsl.str()});
    block.functionNameByNode.emplace(node.get(), functionName);
    block.nodeIdByNode.emplace(node.get(), id);
    emittedIds.insert(id);
}

} // namespace

GlslSdfHelperBlock GlslEmitter::emitSdfHelpers(const SdfNodePtr& root, SdfCompileResult& result) const
{
    GlslSdfHelperBlock block;
    if (!root) {
        result.errors.push_back("Cannot emit SDF helpers for an empty tree.");
        return block;
    }

    std::unordered_map<const SdfNode*, uint64_t> generatedIds;
    std::unordered_set<uint64_t> emittedIds;
    std::unordered_set<const SdfNode*> visiting;
    uint64_t nextGeneratedId = kGeneratedHelperIdBase;
    glsl_emitter::SdfHelperEmitContext context{generatedIds, nextGeneratedId, m_mode};

    emitSdfHelperPostorder(root, result, block, context, emittedIds, visiting);
    block.rootFunctionName = glsl_emitter::helperNameFor(root, context);
    return block;
}

std::string GlslEmitter::emitSceneMaterialExpression(
    const SdfNodePtr& root,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers) const
{
    const MaterialSystem materialSystem;
    materialSystem.ensureDefaultMaterial(result);
    return glsl_emitter::emitMaterialFor(root, pointExpr, result, sdfHelpers, m_mode);
}

std::string GlslEmitter::emitScenePickIdExpression(
    const SdfNodePtr& root,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers) const
{
    return emitPickIdFor(root, pointExpr, result, sdfHelpers, m_mode);
}

} // namespace sdf3d
