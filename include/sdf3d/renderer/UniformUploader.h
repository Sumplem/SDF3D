#pragma once

#include "sdf3d/scene/SdfCompiler.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace sdf3d {

enum class RenderQuality {
    Low = 0,
    Medium = 1,
    High = 2,
};

/// Camera values needed by the raymarch shader.
struct RenderCamera {
    glm::vec3 position = {0.0f, 0.0f, 4.0f};
    glm::vec3 target = {0.0f, 0.0f, 0.0f};
    glm::vec3 up = {0.0f, 1.0f, 0.0f};
    float fovDegrees = 45.0f;
};

/// Gizmo uniforms consumed by edit raymarch shaders.
struct RenderGizmo {
    bool visible = false;
    glm::vec3 center = {0.0f, 0.0f, 0.0f};
    float arrowLength = 1.0f;
    float arrowRadius = 0.035f;
    float ringRadius = 0.85f;
    float tubeRadius = 0.025f;
    int activeAxis = -1;
    int hoverAxis = -1;
    int type = 0;
    int highlightNodeId = 0;
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
    void upload(
        unsigned int program,
        int width,
        int height,
        const RenderCamera& camera,
        const RenderGizmo& gizmo,
        RenderQuality quality,
        const std::vector<SdfCompiledMaterial>& materials);

    /// Returns material count visible to shader storage buffer.
    static size_t materialCountForShader(size_t materialCount);

    /// Packs compiler material data into shader storage buffer layout.
    static std::vector<GpuMaterial> packMaterials(const std::vector<SdfCompiledMaterial>& materials);

private:
    uint32_t m_materialBuffer = 0;
};

} // namespace sdf3d
