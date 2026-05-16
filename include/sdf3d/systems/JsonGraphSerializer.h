#pragma once

#include "sdf3d/systems/GraphSerializer.h"

namespace sdf3d {

/// JSON graph persistence implementation.
class JsonGraphSerializer final : public GraphSerializer {
public:
    bool save(const SdfGraph& graph, const std::filesystem::path& path) override;
    bool load(SdfGraph& graph, const std::filesystem::path& path) override;
};

} // namespace sdf3d
