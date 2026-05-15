#pragma once

#include "sdf3d/scene/SdfCompiler.h"

namespace sdf3d {

/// Orchestrates graph lowering and GLSL scene assembly.
class CompilerSystem {
public:
    /// Compiles a tree into self-contained SDF GLSL.
    SdfCompileResult compile(const SdfNodePtr& root) const;

    /// Compiles a graph output into self-contained SDF GLSL.
    SdfCompileResult compile(const SdfGraph& graph) const;
};

} // namespace sdf3d
