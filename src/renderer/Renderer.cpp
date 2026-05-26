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
    m_pathTraceAccumulation.init();
    m_uniformUploader.init();

    // AGENT: Shader files stay in assets so runtime asset loading and later
    // hot-reload use one path through ShaderManager.
    return m_shaderManager.init(shaderRoot);
}

void Renderer::shutdown()
{
    m_uniformUploader.shutdown();
    m_pathTraceAccumulation.shutdown();
    m_fboRenderer.shutdown();
    m_shaderManager.shutdown();
}

void Renderer::resize(int width, int height)
{
    m_fboRenderer.resize(width, height);
}

void Renderer::render(const RenderCamera& camera)
{
    const bool progressive = m_renderMode == RenderMode::ProgressivePathTrace
        && !m_gizmo.visible
        && m_gizmo.highlightNodeId == 0
        && m_gizmo.hoverNodeId <= 0;
    m_lastFramePathTracing = progressive;
    const GLuint program = progressive ? m_shaderManager.pathTraceProgram() : m_shaderManager.program();
    if (program == 0 || !m_fboRenderer.begin()) {
        m_lastFramePathTracing = false;
        return;
    }

    glUseProgram(program);

    m_uniformUploader.upload(
        program,
        m_fboRenderer.width(),
        m_fboRenderer.height(),
        camera,
        m_gizmo,
        m_quality,
        m_environmentColor,
        m_materials,
        m_nodeParams,
        m_instancePositions,
        m_materialBufferDirty,
        m_nodeParamBufferDirty,
        m_instancePositionBufferDirty);
    m_materialBufferDirty = false;
    m_nodeParamBufferDirty = false;
    m_instancePositionBufferDirty = false;
    bool pathTraceFrameReady = false;
    if (progressive) {
        const PathTraceFrameKey key{
            m_fboRenderer.width(),
            m_fboRenderer.height(),
            camera,
            m_quality,
            m_renderMode,
            m_environmentColor,
            m_sceneRevision,
            m_materialRevision,
            m_nodeParamRevision,
            m_instanceRevision,
        };
        if (m_pathTraceAccumulation.prepareFrame(key)) {
            pathTraceFrameReady = true;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_pathTraceAccumulation.accumulationTexture());
            glUniform1i(glGetUniformLocation(program, "uPathTraceAccumulation"), 0);
            glUniform1ui(glGetUniformLocation(program, "uPathTraceSampleIndex"), m_pathTraceAccumulation.sampleCount());
        }
    }

    m_fboRenderer.drawFullscreenTriangle();
    if (pathTraceFrameReady) {
        glBindTexture(GL_TEXTURE_2D, m_pathTraceAccumulation.accumulationTexture());
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_fboRenderer.width(), m_fboRenderer.height());
        m_pathTraceAccumulation.markSampleRendered();
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glUseProgram(0);
    m_fboRenderer.end();
}

bool Renderer::reloadScene(const std::string& sceneGlsl)
{
    const bool reloaded = m_shaderManager.reloadScene(sceneGlsl);
    if (reloaded) {
        ++m_sceneRevision;
        m_pathTraceAccumulation.reset();
    }
    return reloaded;
}

void Renderer::setGizmo(const RenderGizmo& gizmo)
{
    m_gizmo = gizmo;
}

void Renderer::setQuality(RenderQuality quality)
{
    if (m_quality != quality) {
        m_pathTraceAccumulation.reset();
    }
    m_quality = quality;
}

void Renderer::setRenderMode(RenderMode mode)
{
    if (m_renderMode != mode) {
        m_pathTraceAccumulation.reset();
    }
    m_renderMode = mode;
}

void Renderer::setEnvironmentColor(const glm::vec3& color)
{
    if (m_environmentColor != color) {
        m_pathTraceAccumulation.reset();
    }
    m_environmentColor = color;
}

void Renderer::setMaterials(std::vector<SdfCompiledMaterial> materials)
{
    m_materials = std::move(materials);
    m_materialBufferDirty = true;
    ++m_materialRevision;
    m_pathTraceAccumulation.reset();
}

void Renderer::setNodeParams(std::vector<SdfCompiledNodeParam> nodeParams)
{
    m_nodeParams = std::move(nodeParams);
    m_nodeParamBufferDirty = true;
    ++m_nodeParamRevision;
    m_pathTraceAccumulation.reset();
}

void Renderer::setInstancePositions(std::vector<SdfCompiledInstancePosition> instancePositions)
{
    m_instancePositions = std::move(instancePositions);
    m_instancePositionBufferDirty = true;
    ++m_instanceRevision;
    m_pathTraceAccumulation.reset();
}

const std::string& Renderer::lastError() const
{
    return m_shaderManager.lastError();
}

unsigned int Renderer::outputTexture() const
{
    return m_fboRenderer.outputTexture();
}

int Renderer::readNodeIdPixel(int x, int y) const
{
    return m_fboRenderer.readNodeIdPixel(x, y);
}

uint32_t Renderer::pathTraceSampleCount() const
{
    return m_pathTraceAccumulation.sampleCount();
}

bool Renderer::pathTraceActive() const
{
    return m_lastFramePathTracing;
}

} // namespace sdf3d
