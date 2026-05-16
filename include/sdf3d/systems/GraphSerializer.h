#pragma once

#include "sdf3d/scene/SdfGraph.h"

#include <filesystem>
#include <string>

namespace sdf3d {

/// Abstract graph persistence boundary so file format can change later.
class GraphSerializer {
public:
    virtual ~GraphSerializer() = default;

    virtual bool save(const SdfGraph& graph, const std::filesystem::path& path) = 0;
    virtual bool load(SdfGraph& graph, const std::filesystem::path& path) = 0;

    const std::string& lastError() const;

protected:
    void setLastError(std::string error);
    void clearLastError();

private:
    std::string m_lastError;
};

} // namespace sdf3d
