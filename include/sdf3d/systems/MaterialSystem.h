#pragma once

#include "sdf3d/scene/SdfCompiler.h"
#include "sdf3d/scene/SdfGraph.h"

namespace sdf3d {

class GraphGroupRegistry;

/// Packs SDF materials into deterministic compiler output slots.
class MaterialSystem {
public:
    /// Ensures default scene material exists at material ID 0.
    int ensureDefaultMaterial(SdfCompileResult& result) const;

    /// Appends one material and returns its material ID.
    int appendMaterial(SdfCompileResult& result, const SdfMaterial& material) const;

    /// Collects material uniforms from a lowered tree without emitting GLSL.
    SdfCompileResult collectMaterials(const SdfNodePtr& root) const;

    /// Collects material uniforms from graph output without emitting GLSL.
    SdfCompileResult collectMaterials(const SdfGraph& graph) const;

    /// Collects material uniforms from graph output with resolved groups.
    SdfCompileResult collectMaterials(const SdfGraph& graph, const GraphGroupRegistry& groups) const;
};

} // namespace sdf3d
