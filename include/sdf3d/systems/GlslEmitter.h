#pragma once

#include "sdf3d/scene/SdfCompiler.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sdf3d {

struct GlslSdfHelper {
    uint64_t nodeId = 0;
    std::string functionName;
    std::string glsl;
};

struct GlslSdfHelperBlock {
    std::vector<GlslSdfHelper> helpers;
    std::string rootFunctionName;
    std::unordered_map<const SdfNode*, std::string> functionNameByNode;
    std::unordered_map<const SdfNode*, uint64_t> nodeIdByNode;
};

/// Emits per-node GLSL expressions for SDF compiler orchestration.
class GlslEmitter {
public:
    /// Emits a material-aware GLSL expression for one SDF node subtree.
    std::string emitNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;

    /// Emits geometry-only SDF helpers in dependency order, leaves first.
    GlslSdfHelperBlock emitSdfHelpers(const SdfNodePtr& root, SdfCompileResult& result) const;

    /// Emits material lookup logic that reuses SDF helpers and runs after hit detection.
    std::string emitSceneMaterialExpression(
        const SdfNodePtr& root,
        const std::string& pointExpr,
        SdfCompileResult& result,
        const GlslSdfHelperBlock& sdfHelpers) const;

    /// Emits node-id lookup logic for edit-buffer GPU picking.
    std::string emitScenePickIdExpression(
        const SdfNodePtr& root,
        const std::string& pointExpr,
        SdfCompileResult& result,
        const GlslSdfHelperBlock& sdfHelpers) const;

private:
    std::string emitPrimitiveNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
    std::string emitBooleanNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
    std::string emitDomainNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
    std::string emitMaterialNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
};

} // namespace sdf3d
