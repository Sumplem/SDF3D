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

std::string nodePickIdLiteral(
    const SdfNodePtr& node,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers)
{
    return std::to_string(static_cast<int>(helperNodeIdFor(node, result, sdfHelpers)));
}

struct SdfWithIdEval {
    std::string statements;
    std::string expression;
};

struct SdfWithIdContext {
    int nextTemp = 0;
};

std::string nextSdfWithIdTemp(SdfWithIdContext& context)
{
    return "sdf3d_hit" + std::to_string(context.nextTemp++);
}

std::string emitSdfWithIdTemp(std::ostringstream& statements, SdfWithIdContext& context, const std::string& expression)
{
    const std::string name = nextSdfWithIdTemp(context);
    statements << "    vec2 " << name << " = " << expression << ";\n";
    return name;
}

SdfWithIdEval emitSdfWithIdFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode,
    SdfWithIdContext& context);

SdfWithIdEval emitDomainSdfWithIdFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode,
    SdfWithIdContext& context)
{
    using namespace glsl_emitter;

    if (node->children.empty()) {
        result.errors.push_back(glslNodeTypeName(node->type) + " node has no child.");
        return {"", "vec2(1e6, -1.0)"};
    }
    if (node->children.size() > 1) {
        result.errors.push_back(glslNodeTypeName(node->type) + " node ignores extra children.");
    }

    switch (node->type) {
    case SdfNodeType::Translate:
    case SdfNodeType::Rotate: {
        const std::string distance = helperDistanceFor(node, pointExpr, result, sdfHelpers);
        return {"", "vec2(" + distance + ", " + nodePickIdLiteral(node, result, sdfHelpers) + ".0)"};
    }
    case SdfNodeType::Scale: {
        const std::string distance = helperDistanceFor(node, pointExpr, result, sdfHelpers);
        return {"", "vec2(" + distance + ", " + nodePickIdLiteral(node, result, sdfHelpers) + ".0)"};
    }
    case SdfNodeType::Repeat:
        return emitSdfWithIdFor(node->children.front(), repeatedPointFor(*node, node->stableId, mode, pointExpr, sdfHelpers.nodeParamSlotByNodeId), result, sdfHelpers, mode, context);
    case SdfNodeType::Mirror:
        return emitSdfWithIdFor(node->children.front(), mirroredPointFor(*node, pointExpr), result, sdfHelpers, mode, context);
    case SdfNodeType::Twist:
    case SdfNodeType::Bend: {
        const DomainWarpExpr warp = domainWarpFor(*node, node->stableId, mode, pointExpr, sdfHelpers.nodeParamSlotByNodeId);
        SdfWithIdEval child = emitSdfWithIdFor(node->children.front(), warp.point, result, sdfHelpers, mode, context);
        std::ostringstream statements;
        statements << child.statements;
        const std::string childHit = emitSdfWithIdTemp(statements, context, child.expression);
        return {statements.str(), "vec2(" + childHit + ".x / " + warp.correction + ", " + childHit + ".y)"};
    }
    default:
        break;
    }

    return {"", "vec2(1e6, -1.0)"};
}

SdfWithIdEval emitBooleanSdfWithIdFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode,
    SdfWithIdContext& context)
{
    using namespace glsl_emitter;

    if (node->children.empty()) {
        result.errors.push_back(glslNodeTypeName(node->type) + " node has no children.");
        return {"", "vec2(1e6, -1.0)"};
    }
    if (node->children.size() == 1) {
        return emitSdfWithIdFor(node->children.front(), pointExpr, result, sdfHelpers, mode, context);
    }

    const float smoothnessValue = std::max(parameterOr(*node, "smoothness", 0.25f), 0.0001f);
    const std::string smoothness = "max(" + glslNodeParamComponent(mode, node->stableId, glslVec4(smoothnessValue, 0.0f, 0.0f, 0.0f), 'x', sdfHelpers.nodeParamSlotByNodeId) + ", 0.000100)";
    std::ostringstream statements;

    SdfWithIdEval baseEval = emitSdfWithIdFor(node->children.front(), pointExpr, result, sdfHelpers, mode, context);
    statements << baseEval.statements;
    std::string current = emitSdfWithIdTemp(statements, context, baseEval.expression);

    if (node->type == SdfNodeType::Subtract || node->type == SdfNodeType::SmoothSubtract) {
        SdfWithIdEval cutterEval = emitSdfWithIdFor(node->children[1], pointExpr, result, sdfHelpers, mode, context);
        statements << cutterEval.statements;
        const std::string cutter = emitSdfWithIdTemp(statements, context, cutterEval.expression);
        if (node->type == SdfNodeType::Subtract) {
            current = emitSdfWithIdTemp(statements, context, "vec2(max(-(" + cutter + ".x), " + current + ".x), " + current + ".y)");
        } else {
            current = emitSdfWithIdTemp(statements, context, "vec2(-sdf3d_smin(-(" + current + ".x), " + cutter + ".x, " + smoothness + "), " + current + ".y)");
        }
        if (node->children.size() > 2) {
            result.errors.push_back(glslNodeTypeName(node->type) + " node ignores extra children beyond base and cutter.");
        }
        return {statements.str(), current};
    }

    const bool useMax = node->type == SdfNodeType::Intersect || node->type == SdfNodeType::SmoothIntersect;
    const bool smoothUnion = node->type == SdfNodeType::SmoothUnion;
    const bool smoothIntersect = node->type == SdfNodeType::SmoothIntersect;
    for (std::size_t i = 1; i < node->children.size(); ++i) {
        SdfWithIdEval childEval = emitSdfWithIdFor(node->children[i], pointExpr, result, sdfHelpers, mode, context);
        statements << childEval.statements;
        const std::string child = emitSdfWithIdTemp(statements, context, childEval.expression);
        if (smoothUnion) {
            const std::string blend = "clamp(0.5 + 0.5 * (" + child + ".x - " + current + ".x) / " + smoothness + ", 0.0, 1.0)";
            current = emitSdfWithIdTemp(statements, context, "vec2(sdf3d_smin(" + current + ".x, " + child + ".x, " + smoothness + "), (" + blend + " > 0.5 ? " + current + ".y : " + child + ".y))");
        } else if (smoothIntersect) {
            const std::string blend = "clamp(0.5 + 0.5 * (" + current + ".x - " + child + ".x) / " + smoothness + ", 0.0, 1.0)";
            current = emitSdfWithIdTemp(statements, context, "vec2(-sdf3d_smin(-(" + current + ".x), -(" + child + ".x), " + smoothness + "), (" + blend + " > 0.5 ? " + current + ".y : " + child + ".y))");
        } else {
            const std::string chooseFirst = current + (useMax ? ".x > " : ".x < ") + child + ".x";
            current = emitSdfWithIdTemp(statements, context, "vec2(" + std::string(useMax ? "max(" : "min(") + current + ".x, " + child + ".x), (" + chooseFirst + " ? " + current + ".y : " + child + ".y))");
        }
    }

    return {statements.str(), current};
}

SdfWithIdEval emitSdfWithIdFor(
    const SdfNodePtr& node,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    GlslEmitMode mode,
    SdfWithIdContext& context)
{
    if (!node) {
        result.errors.push_back("Encountered a null SDF node while emitting SDF-with-id evaluation.");
        return {"", "vec2(1e6, -1.0)"};
    }
    if (isSdfPrimitiveNode(node->type)) {
        return {"", "vec2(" + helperDistanceFor(node, pointExpr, result, sdfHelpers) + ", " + nodePickIdLiteral(node, result, sdfHelpers) + ".0)"};
    }
    if (isSdfBooleanNode(node->type)) {
        return emitBooleanSdfWithIdFor(node, pointExpr, result, sdfHelpers, mode, context);
    }
    if (isSdfTransformNode(node->type)) {
        return emitDomainSdfWithIdFor(node, pointExpr, result, sdfHelpers, mode, context);
    }
    if (node->type == SdfNodeType::MaterialOverride) {
        if (node->children.empty()) {
            result.errors.push_back("MaterialOverride node has no SDF input.");
            return {"", "vec2(1e6, -1.0)"};
        }
        return emitSdfWithIdFor(node->children.front(), pointExpr, result, sdfHelpers, mode, context);
    }
    if (node->type == SdfNodeType::Group) {
        return {"", "vec2(" + helperDistanceFor(node, pointExpr, result, sdfHelpers) + ", " + nodePickIdLiteral(node, result, sdfHelpers) + ".0)"};
    }

    result.errors.push_back("Unsupported SDF node type in SDF-with-id evaluation: " + glslNodeTypeName(node->type));
    return {"", "vec2(1e6, -1.0)"};
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
    for (const SdfCompiledNodeParam& param : result.nodeParams) {
        block.nodeParamSlotByNodeId[param.nodeId] = param.slot;
    }
    if (!root) {
        result.errors.push_back("Cannot emit SDF helpers for an empty tree.");
        return block;
    }

    std::unordered_map<const SdfNode*, uint64_t> generatedIds;
    std::unordered_set<uint64_t> emittedIds;
    std::unordered_set<const SdfNode*> visiting;
    uint64_t nextGeneratedId = kGeneratedHelperIdBase;
    glsl_emitter::SdfHelperEmitContext context{generatedIds, block.nodeParamSlotByNodeId, nextGeneratedId, m_mode};

    emitSdfHelperPostorder(root, result, block, context, emittedIds, visiting);
    block.rootFunctionName = glsl_emitter::helperNameFor(root, context);
    return block;
}

std::string GlslEmitter::emitSceneMaterialBody(
    const SdfNodePtr& root,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers,
    const MaterialSystem& materialSystem) const
{
    materialSystem.ensureDefaultMaterial(result);
    return glsl_emitter::emitMaterialBodyFor(root, pointExpr, result, sdfHelpers, m_mode, materialSystem);
}

std::string GlslEmitter::emitSceneSdfWithIdBody(
    const SdfNodePtr& root,
    const std::string& pointExpr,
    SdfCompileResult& result,
    const GlslSdfHelperBlock& sdfHelpers) const
{
    SdfWithIdContext context;
    const SdfWithIdEval eval = emitSdfWithIdFor(root, pointExpr, result, sdfHelpers, m_mode, context);
    std::ostringstream body;
    body << eval.statements;
    body << "    return " << eval.expression << ";\n";
    return body.str();
}

} // namespace sdf3d
