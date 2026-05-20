#pragma once

#include "sdf3d/scene/SdfGraph.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace sdf3d {

/// Reusable graph definition referenced by Group instance nodes.
struct GraphGroupDefinition {
    GroupDefId id = 0;
    std::string name;
    SdfGraph subgraph;
};

/// Owns reusable graph group definitions outside the instance graph.
class GraphGroupRegistry {
public:
    /// Creates a group definition and returns its stable ID.
    GroupDefId createDefinition(std::string name, SdfGraph subgraph)
    {
        const GroupDefId id = m_nextId++;
        m_definitions.push_back({id, std::move(name), std::move(subgraph)});
        return id;
    }

    /// Returns one group definition by ID, or nullptr when missing.
    GraphGroupDefinition* definition(GroupDefId id)
    {
        const auto it = std::find_if(m_definitions.begin(), m_definitions.end(), [id](const GraphGroupDefinition& definition) {
            return definition.id == id;
        });
        return it == m_definitions.end() ? nullptr : &*it;
    }

    /// Returns one group definition by ID, or nullptr when missing.
    const GraphGroupDefinition* definition(GroupDefId id) const
    {
        const auto it = std::find_if(m_definitions.begin(), m_definitions.end(), [id](const GraphGroupDefinition& definition) {
            return definition.id == id;
        });
        return it == m_definitions.end() ? nullptr : &*it;
    }

    /// Returns all group definitions in registry order.
    const std::vector<GraphGroupDefinition>& definitions() const
    {
        return m_definitions;
    }

    /// Replaces registry contents after validating serialized definitions.
    bool replaceDefinitions(std::vector<GraphGroupDefinition> definitions, GroupDefId nextId)
    {
        if (nextId == 0) {
            return false;
        }

        GroupDefId maxId = 0;
        std::vector<GroupDefId> ids;
        ids.reserve(definitions.size());
        for (const GraphGroupDefinition& definition : definitions) {
            if (definition.id == 0 || definition.name.empty() || definition.subgraph.outputNode() == 0) {
                return false;
            }
            maxId = std::max(maxId, definition.id);
            if (std::find(ids.begin(), ids.end(), definition.id) != ids.end()) {
                return false;
            }
            ids.push_back(definition.id);
        }

        if (nextId <= maxId) {
            return false;
        }

        m_definitions = std::move(definitions);
        m_nextId = nextId;
        return true;
    }

    /// Returns next stable group definition ID for serialization.
    GroupDefId nextDefinitionIdForSerialization() const
    {
        return m_nextId;
    }

private:
    GroupDefId m_nextId = 1;
    std::vector<GraphGroupDefinition> m_definitions;
};

} // namespace sdf3d
