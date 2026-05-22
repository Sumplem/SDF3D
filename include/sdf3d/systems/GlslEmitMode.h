#pragma once

namespace sdf3d {

/// Selects whether emitted GLSL reads runtime node parameters or bakes literals.
enum class GlslEmitMode {
    Runtime,
    Baked,
};

} // namespace sdf3d
