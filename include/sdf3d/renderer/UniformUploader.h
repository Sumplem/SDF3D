#pragma once

#include "sdf3d/scene/SdfCompiler.h"

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

namespace sdf3d {

/// Camera values needed by the raymarch shader.
struct RenderCamera {
    glm::vec3 position = {0.0f, 0.0f, 4.0f};
    glm::vec3 target = {0.0f, 0.0f, 0.0f};
    glm::vec3 up = {0.0f, 1.0f, 0.0f};
    float fovDegrees = 45.0f;
};

/// Uploads camera, viewport, and material uniforms to the active shader.
class UniformUploader {
public:
    /// Uploads all raymarch uniforms for one frame.
    void upload(unsigned int program, int width, int height, const RenderCamera& camera, const std::vector<SdfCompiledMaterial>& materials) const;

    /// Returns material count capped to shader uniform array capacity.
    static size_t materialCountForShader(size_t materialCount);
};

} // namespace sdf3d
