#pragma once

#include "sdf3d/scene/SdfCompiler.h"
#include "sdf3d/systems/GlslEmitMode.h"

#include <vector>

namespace sdf3d {

class GraphGroupRegistry;

/// Orchestrates graph lowering and GLSL scene assembly.
class CompilerSystem {
public:
    /// Compiles a tree into self-contained SDF GLSL.
    SdfCompileResult compile(const SdfNodePtr& root, GlslEmitMode mode = GlslEmitMode::Runtime) const;

    /// Compiles a graph output into self-contained SDF GLSL.
    SdfCompileResult compile(const SdfGraph& graph, GlslEmitMode mode = GlslEmitMode::Runtime) const;

    /// Compiles a graph output with resolved group definitions.
    SdfCompileResult compile(const SdfGraph& graph, const GraphGroupRegistry& groups, GlslEmitMode mode = GlslEmitMode::Runtime) const;

private:
    SdfCompileResult compileTree(const SdfNodePtr& root, GlslEmitMode mode, std::vector<SdfCompiledNodeParam> nodeParams) const;
};

} // namespace sdf3d
