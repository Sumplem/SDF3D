#pragma once

#include "sdf3d/scene/SdfCompiler.h"

namespace sdf3d {

class GraphGroupRegistry;

/// Orchestrates graph lowering and GLSL scene assembly.
class CompilerSystem {
public:
    /// Compiles a tree into self-contained SDF GLSL.
    SdfCompileResult compile(const SdfNodePtr& root) const;

    /// Compiles a graph output into self-contained SDF GLSL.
    SdfCompileResult compile(const SdfGraph& graph) const;

    /// Compiles a graph output with resolved group definitions.
    SdfCompileResult compile(const SdfGraph& graph, const GraphGroupRegistry& groups) const;
};

} // namespace sdf3d
