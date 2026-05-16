#pragma once

#include "sdf3d/scene/SdfCompiler.h"

#include <cstddef>
#include <cstdint>
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
    struct GpuMaterial {
        glm::vec4 albedoRoughness = {0.8f, 0.8f, 0.8f, 0.5f};
        glm::vec4 metallicEmission = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    UniformUploader() = default;
    ~UniformUploader();

    UniformUploader(const UniformUploader&) = delete;
    UniformUploader& operator=(const UniformUploader&) = delete;

    /// Allocates renderer-owned material buffer object.
    void init();

    /// Releases renderer-owned material buffer object.
    void shutdown();

    /// Uploads all raymarch uniforms for one frame.
    void upload(unsigned int program, int width, int height, const RenderCamera& camera, const std::vector<SdfCompiledMaterial>& materials);

    /// Returns material count visible to shader storage buffer.
    static size_t materialCountForShader(size_t materialCount);

    /// Packs compiler material data into shader storage buffer layout.
    static std::vector<GpuMaterial> packMaterials(const std::vector<SdfCompiledMaterial>& materials);

private:
    uint32_t m_materialBuffer = 0;
};

} // namespace sdf3d
