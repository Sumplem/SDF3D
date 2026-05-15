#include "sdf3d/renderer/Renderer.h"

#include <string>
#include <utility>

#include <glad/gl.h>

namespace sdf3d {

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init(const std::filesystem::path& shaderRoot)
{
    m_fboRenderer.init();

    // AGENT: Shader files stay in assets so runtime asset loading and later
    // hot-reload use one path through ShaderManager.
    return m_shaderManager.init(shaderRoot);
}

void Renderer::shutdown()
{
    m_fboRenderer.shutdown();
    m_shaderManager.shutdown();
}

void Renderer::resize(int width, int height)
{
    m_fboRenderer.resize(width, height);
}

void Renderer::render(const RenderCamera& camera)
{
    const GLuint program = m_shaderManager.program();
    if (program == 0 || !m_fboRenderer.begin()) {
        return;
    }

    glUseProgram(program);

    m_uniformUploader.upload(program, m_fboRenderer.width(), m_fboRenderer.height(), camera, m_materials);

    m_fboRenderer.drawFullscreenTriangle();

    glUseProgram(0);
    m_fboRenderer.end();
}

bool Renderer::reloadScene(const std::string& sceneGlsl)
{
    return m_shaderManager.reloadScene(sceneGlsl);
}

void Renderer::setMaterials(std::vector<SdfCompiledMaterial> materials)
{
    m_materials = std::move(materials);
}

const std::string& Renderer::lastError() const
{
    return m_shaderManager.lastError();
}

unsigned int Renderer::outputTexture() const
{
    return m_fboRenderer.outputTexture();
}

} // namespace sdf3d
