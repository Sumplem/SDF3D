#include "sdf3d/systems/MaterialSystem.h"

namespace sdf3d {

int MaterialSystem::ensureDefaultMaterial(SdfCompileResult& result) const
{
    if (result.materials.empty()) {
        result.materials.push_back({SdfMaterial{}});
    }

    return 0;
}

int MaterialSystem::appendMaterial(SdfCompileResult& result, const SdfMaterial& material) const
{
    ensureDefaultMaterial(result);
    const int id = static_cast<int>(result.materials.size());
    result.materials.push_back({material});
    return id;
}

} // namespace sdf3d
