#pragma once

#include "sdf3d/renderer/FboRenderer.h"
#include "sdf3d/renderer/PathTraceAccumulation.h"
#include "sdf3d/renderer/ShaderManager.h"
#include "sdf3d/renderer/UniformUploader.h"
#include "sdf3d/scene/SdfCompiler.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace sdf3d {

/// Owns the M2 fullscreen raymarch draw path.
class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Compiles the raymarch shader program from runtime assets.
    bool init(const std::filesystem::path& shaderRoot);

    /// Releases OpenGL resources owned by the renderer.
    void shutdown();

    /// Updates the output viewport dimensions used for rendering.
    void resize(int width, int height);

    /// Draws the hardcoded M2 raymarched scene.
    void render(const RenderCamera& camera);

    /// Stores edit gizmo uniforms for the next render.
    void setGizmo(const RenderGizmo& gizmo);

    /// Stores editor quality for the next render.
    void setQuality(RenderQuality quality);

    /// Stores render mode for the next render.
    void setRenderMode(RenderMode mode);

    /// Stores path-trace environment lighting color.
    void setEnvironmentColor(const glm::vec3& color);

    /// Rebuilds the fragment shader after replacing the sceneSDF injection block.
    bool reloadScene(const std::string& sceneGlsl);

    /// Stores material uniforms emitted by the SDF compiler.
    void setMaterials(std::vector<SdfCompiledMaterial> materials);

    /// Stores node parameters emitted by the SDF compiler or refreshed from graph edits.
    void setNodeParams(std::vector<SdfCompiledNodeParam> nodeParams);

    /// Stores instanced primitive positions emitted by the SDF compiler or refreshed from graph edits.
    void setInstancePositions(std::vector<SdfCompiledInstancePosition> instancePositions);

    /// Returns the latest shader compile/link/reload error, or empty on success.
    const std::string& lastError() const;

    /// Returns the color texture containing the most recent viewport render.
    unsigned int outputTexture() const;

    /// Reads a node id from the most recent edit picking attachment.
    int readNodeIdPixel(int x, int y) const;

    /// Returns progressive path-trace samples accumulated for current frame key.
    uint32_t pathTraceSampleCount() const;

    /// Returns true when the last rendered frame used progressive path tracing.
    bool pathTraceActive() const;

private:
    FboRenderer m_fboRenderer;
    PathTraceAccumulation m_pathTraceAccumulation;
    ShaderManager m_shaderManager;
    UniformUploader m_uniformUploader;
    RenderGizmo m_gizmo;
    RenderQuality m_quality = RenderQuality::High;
    RenderMode m_renderMode = RenderMode::DirectPreview;
    glm::vec3 m_environmentColor = {0.46f, 0.56f, 0.72f};
    bool m_lastFramePathTracing = false;
    uint64_t m_sceneRevision = 0;
    uint64_t m_materialRevision = 0;
    uint64_t m_nodeParamRevision = 0;
    uint64_t m_instanceRevision = 0;
    bool m_materialBufferDirty = true;
    bool m_nodeParamBufferDirty = true;
    bool m_instancePositionBufferDirty = true;
    // AGENT: Renderer stores compiler-owned material order so every render can
    // re-upload uniforms after program relink without scene graph traversal.
    std::vector<SdfCompiledMaterial> m_materials;
    std::vector<SdfCompiledNodeParam> m_nodeParams;
    std::vector<SdfCompiledInstancePosition> m_instancePositions;
};

} // namespace sdf3d
