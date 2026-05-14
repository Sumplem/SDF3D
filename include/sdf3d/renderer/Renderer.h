#pragma once

#include "sdf3d/scene/SdfCompiler.h"

#include <filesystem>
#include <string>
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

    /// Rebuilds the fragment shader after replacing the sceneSDF injection block.
    bool reloadScene(const std::string& sceneGlsl);

    /// Stores material uniforms emitted by the SDF compiler.
    void setMaterials(std::vector<SdfCompiledMaterial> materials);

    /// Returns the color texture containing the most recent viewport render.
    unsigned int outputTexture() const;

private:
    bool loadProgram(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
    bool loadProgramFromSources(const std::string& vertexSource, const std::string& fragmentSource);
    std::string fragmentSourceWithScene(const std::string& sceneGlsl) const;
    void resizeFramebuffer();

    std::filesystem::path m_vertexShaderPath;
    std::filesystem::path m_fragmentShaderPath;
    unsigned int m_program = 0;
    unsigned int m_vertexArray = 0;
    unsigned int m_framebuffer = 0;
    unsigned int m_colorTexture = 0;
    int m_width = 1;
    int m_height = 1;
    // AGENT: Renderer stores compiler-owned material order so every render can
    // re-upload uniforms after program relink without scene graph traversal.
    std::vector<SdfCompiledMaterial> m_materials;
};

} // namespace sdf3d
