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

enum class RenderMode {
    DirectPreview = 0,
    ProgressivePathTrace = 1,
};

enum class GizmoRotateStyle {
    Rings = 0,
    AxisArcs = 1,
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
    glm::mat3 orientation = glm::mat3{1.0f};
    float arrowLength = 1.0f;
    float arrowRadius = 0.035f;
    float ringRadius = 0.85f;
    float tubeRadius = 0.025f;
    int activeAxis = -1;
    int hoverAxis = -1;
    int type = 0;
    GizmoRotateStyle rotateStyle = GizmoRotateStyle::Rings;
    int highlightNodeId = 0;
    int hoverNodeId = -1;
};

/// Uploads camera, viewport, and material uniforms to the active shader.
class UniformUploader {
public:
    struct GpuMaterial {
        glm::vec4 albedoRoughness = {0.8f, 0.8f, 0.8f, 0.5f};
        glm::vec4 metallicEmissionType = {0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 secondaryAlbedoScale = {0.08f, 0.08f, 0.08f, 4.0f};
    };

    struct GpuNodeParam {
        glm::vec4 data0 = {0.0f, 0.0f, 0.0f, 0.0f};
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
        const glm::vec3& environmentColor,
        const std::vector<SdfCompiledMaterial>& materials,
        const std::vector<SdfCompiledNodeParam>& nodeParams);

    /// Returns material count visible to shader storage buffer.
    static size_t materialCountForShader(size_t materialCount);

    /// Packs compiler material data into shader storage buffer layout.
    static std::vector<GpuMaterial> packMaterials(const std::vector<SdfCompiledMaterial>& materials);

    /// Packs compiler node parameters into shader storage buffer layout.
    static std::vector<GpuNodeParam> packNodeParams(const std::vector<SdfCompiledNodeParam>& nodeParams);

    /// Maps editor quality to stochastic path-trace bounce count.
    static int pathTraceMaxBouncesForQuality(RenderQuality quality);

private:
    uint32_t m_materialBuffer = 0;
    uint32_t m_nodeParamBuffer = 0;
};

} // namespace sdf3d
