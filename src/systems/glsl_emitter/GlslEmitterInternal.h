#pragma once

#include "sdf3d/scene/SdfCompiler.h"
#include "sdf3d/systems/GlslEmitter.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace sdf3d::glsl_emitter {

struct SdfHelperEmitContext {
    std::unordered_map<const SdfNode*, uint64_t>& generatedIds;
    uint64_t& nextGeneratedId;
    GlslEmitMode mode = GlslEmitMode::Runtime;
};

uint64_t helperIdFor(const SdfNodePtr& node, SdfHelperEmitContext& context);
uint64_t runtimeParamIdFor(const SdfNodePtr& node);
std::string helperNameFor(const SdfNodePtr& node, SdfHelperEmitContext& context);
std::string helperCallFor(const SdfNodePtr& node, const std::string& pointExpr, SdfHelperEmitContext& context);

std::string emitGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context);
std::string emitPrimitiveGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context);
std::string emitBooleanGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context);
std::string emitDomainGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context);
std::string emitMaterialGeometryExpression(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, SdfHelperEmitContext& context);
std::string emitMaterialFor(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result, const GlslSdfHelperBlock& sdfHelpers, GlslEmitMode mode);

} // namespace sdf3d::glsl_emitter
