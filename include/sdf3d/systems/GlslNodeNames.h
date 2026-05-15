#pragma once

#include "sdf3d/scene/SdfNode.h"

#include <string>

namespace sdf3d {

/// Returns stable diagnostic text for an SDF node type.
std::string glslNodeTypeName(SdfNodeType type);

} // namespace sdf3d
