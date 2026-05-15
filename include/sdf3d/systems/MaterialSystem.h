#pragma once

#include "sdf3d/scene/SdfCompiler.h"

namespace sdf3d {

/// Packs SDF materials into deterministic compiler output slots.
class MaterialSystem {
public:
    /// Ensures default scene material exists at material ID 0.
    int ensureDefaultMaterial(SdfCompileResult& result) const;

    /// Appends one material and returns its material ID.
    int appendMaterial(SdfCompileResult& result, const SdfMaterial& material) const;
};

} // namespace sdf3d
