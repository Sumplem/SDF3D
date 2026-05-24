#pragma once

#include "sdf3d/scene/SdfCompiler.h"
#include "sdf3d/systems/GlslEmitMode.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sdf3d {

class MaterialSystem;

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
    std::unordered_map<uint64_t, uint32_t> nodeParamSlotByNodeId;
};

/// Emits per-node GLSL expressions for SDF compiler orchestration.
class GlslEmitter {
public:
    /// Creates an emitter for runtime-edit or baked-export GLSL.
    explicit GlslEmitter(GlslEmitMode mode = GlslEmitMode::Runtime);

    /// Emits geometry-only SDF helpers in dependency order, leaves first.
    GlslSdfHelperBlock emitSdfHelpers(const SdfNodePtr& root, SdfCompileResult& result) const;

    /// Emits sceneMaterial body statements that reuse SDF helpers and run after hit detection.
    std::string emitSceneMaterialBody(
        const SdfNodePtr& root,
        const std::string& pointExpr,
        SdfCompileResult& result,
        const GlslSdfHelperBlock& sdfHelpers,
        const MaterialSystem& materialSystem) const;

    /// Emits node-id lookup logic for edit-buffer GPU picking.
    std::string emitSceneSdfWithIdBody(
        const SdfNodePtr& root,
        const std::string& pointExpr,
        SdfCompileResult& result,
        const GlslSdfHelperBlock& sdfHelpers) const;

private:
    GlslEmitMode m_mode = GlslEmitMode::Runtime;
};

} // namespace sdf3d
