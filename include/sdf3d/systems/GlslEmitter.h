#pragma once

#include "sdf3d/scene/SdfCompiler.h"

#include <string>

namespace sdf3d {

/// Emits per-node GLSL expressions for SDF compiler orchestration.
class GlslEmitter {
public:
    /// Emits a material-aware GLSL expression for one SDF node subtree.
    std::string emitNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;

private:
    std::string emitPrimitiveNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
    std::string emitBooleanNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
    std::string emitDomainNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
    std::string emitMaterialNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
};

} // namespace sdf3d
