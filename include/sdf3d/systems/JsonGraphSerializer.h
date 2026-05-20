#pragma once

#include "sdf3d/systems/GraphSerializer.h"

namespace sdf3d {

/// JSON graph persistence implementation.
class JsonGraphSerializer final : public GraphSerializer {
public:
    bool save(const SdfGraph& graph, const std::filesystem::path& path) override;
    bool load(SdfGraph& graph, const std::filesystem::path& path) override;

    /// Saves graph plus root-level group definitions.
    bool save(const SdfGraph& graph, const GraphGroupRegistry& groups, const std::filesystem::path& path);

    /// Loads graph plus root-level group definitions.
    bool load(SdfGraph& graph, GraphGroupRegistry& groups, const std::filesystem::path& path);
};

} // namespace sdf3d
