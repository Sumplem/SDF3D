#include "sdf3d/scene/SdfCompiler.h"

#include "sdf3d/systems/CompilerSystem.h"

namespace sdf3d {

SdfCompileResult SdfCompiler::compile(const SdfGraph& graph) const
{
    const CompilerSystem compiler;
    return compiler.compile(graph);
}

SdfCompileResult SdfCompiler::compile(const SdfNodePtr& root) const
{
    const CompilerSystem compiler;
    return compiler.compile(root);
}

} // namespace sdf3d
