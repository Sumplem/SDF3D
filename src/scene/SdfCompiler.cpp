#include "sdf3d/scene/SdfCompiler.h"

#include "sdf3d/systems/CompilerSystem.h"

namespace sdf3d {

SdfCompileResult SdfCompiler::compile(const SdfGraph& graph, GlslEmitMode mode) const
{
    const CompilerSystem compiler;
    return compiler.compile(graph, mode);
}

SdfCompileResult SdfCompiler::compile(const SdfGraph& graph, const GraphGroupRegistry& groups, GlslEmitMode mode) const
{
    const CompilerSystem compiler;
    return compiler.compile(graph, groups, mode);
}

SdfCompileResult SdfCompiler::compile(const SdfNodePtr& root, GlslEmitMode mode) const
{
    const CompilerSystem compiler;
    return compiler.compile(root, mode);
}

} // namespace sdf3d
