#pragma once

#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/scene/SdfNode.h"

#include <string>
#include <vector>

namespace sdf3d {

/// Material data emitted beside generated GLSL for renderer upload.
struct SdfCompiledMaterial {
    SdfMaterial material;
};

/// Result of compiling an SDF node tree into GLSL.
struct SdfCompileResult {
    std::string glsl;
    std::vector<std::string> errors;
    // AGENT: Material list stays beside GLSL so the compiler owns deterministic
    // material ID assignment without coupling SdfNode to renderer uniform layout.
    std::vector<SdfCompiledMaterial> materials;
    bool usesBox = false;
    bool usesCylinder = false;
    bool usesSmoothMin = false;
    bool usesRotate = false;
};

/// Emits deterministic GLSL for an SDF expression tree.
class SdfCompiler {
public:
    /// Compiles a tree into a self-contained `float sceneSDF(vec3 p)` function.
    SdfCompileResult compile(const SdfNodePtr& root) const;

    /// Compiles a graph output into self-contained SDF GLSL.
    SdfCompileResult compile(const SdfGraph& graph) const;

private:
    std::string compileNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
    std::string compilePrimitiveNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
    std::string compileBooleanNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
    std::string compileDomainNode(const SdfNodePtr& node, const std::string& pointExpr, SdfCompileResult& result) const;
};

} // namespace sdf3d
