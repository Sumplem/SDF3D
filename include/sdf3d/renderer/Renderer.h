#pragma once

#include "sdf3d/renderer/FboRenderer.h"
#include "sdf3d/renderer/ShaderManager.h"
#include "sdf3d/renderer/UniformUploader.h"
#include "sdf3d/scene/SdfCompiler.h"

#include <filesystem>
#include <string>
#include <vector>

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

    /// Rebuilds the fragment shader after replacing the sceneSDF injection block.
    bool reloadScene(const std::string& sceneGlsl);

    /// Stores material uniforms emitted by the SDF compiler.
    void setMaterials(std::vector<SdfCompiledMaterial> materials);

    /// Returns the latest shader compile/link/reload error, or empty on success.
    const std::string& lastError() const;

    /// Returns the color texture containing the most recent viewport render.
    unsigned int outputTexture() const;

private:
    FboRenderer m_fboRenderer;
    ShaderManager m_shaderManager;
    UniformUploader m_uniformUploader;
    RenderGizmo m_gizmo;
    RenderQuality m_quality = RenderQuality::High;
    // AGENT: Renderer stores compiler-owned material order so every render can
    // re-upload uniforms after program relink without scene graph traversal.
    std::vector<SdfCompiledMaterial> m_materials;
};

} // namespace sdf3d
