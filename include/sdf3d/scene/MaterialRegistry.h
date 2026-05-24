#pragma once

#include "sdf3d/components/SdfMaterial.h"
#include "sdf3d/scene/MaterialGraph.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sdf3d {

using MaterialId = uint64_t;

/// Reusable scene material with a stable project ID.
struct MaterialDefinition {
    MaterialId id = 0;
    std::string name;
    SdfMaterial material;
    MaterialGraph graph;
};

/// Owns reusable scene materials independently from graph nodes.
class MaterialRegistry {
public:
    /// Creates a material with a stable ID and returns that ID.
    MaterialId createMaterial(std::string name, SdfMaterial material = {})
    {
        const MaterialId id = m_nextId++;
        m_materials.push_back({id, std::move(name), material, makeMaterialGraphFromMaterial(material)});
        return id;
    }

    /// Removes a material by stable ID.
    bool removeMaterial(MaterialId id)
    {
        const auto it = std::find_if(m_materials.begin(), m_materials.end(), [id](const MaterialDefinition& material) {
            return material.id == id;
        });
        if (it == m_materials.end()) {
            return false;
        }
        m_materials.erase(it);
        return true;
    }

    /// Returns mutable material data by stable ID.
    MaterialDefinition* material(MaterialId id)
    {
        for (MaterialDefinition& definition : m_materials) {
            if (definition.id == id) {
                return &definition;
            }
        }
        return nullptr;
    }

    /// Returns material data by stable ID.
    const MaterialDefinition* material(MaterialId id) const
    {
        for (const MaterialDefinition& definition : m_materials) {
            if (definition.id == id) {
                return &definition;
            }
        }
        return nullptr;
    }

    /// Returns materials in deterministic creation/load order.
    const std::vector<MaterialDefinition>& materials() const
    {
        return m_materials;
    }

    /// Returns next stable material ID for serialization.
    MaterialId nextMaterialIdForSerialization() const
    {
        return m_nextId;
    }

    /// Replaces registry data from validated serialized input.
    bool replaceMaterials(std::vector<MaterialDefinition> materials, MaterialId nextId)
    {
        if (nextId == 0) {
            return false;
        }

        MaterialId maxId = 0;
        std::unordered_set<MaterialId> ids;
        for (const MaterialDefinition& material : materials) {
            if (material.id == 0 || !ids.insert(material.id).second) {
                return false;
            }
            maxId = std::max(maxId, material.id);
        }
        if (nextId <= maxId) {
            return false;
        }

        m_materials = std::move(materials);
        m_nextId = nextId;
        return true;
    }

private:
    MaterialId m_nextId = 1;
    std::vector<MaterialDefinition> m_materials;
};

} // namespace sdf3d
