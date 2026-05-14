#pragma once

#include <filesystem>

namespace sdf3d {

/// Returns the directory containing the running executable.
std::filesystem::path executableDir();

} // namespace sdf3d
