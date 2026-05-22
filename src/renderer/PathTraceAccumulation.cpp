#include "sdf3d/renderer/PathTraceAccumulation.h"

#include "sdf3d/renderer/FboRenderer.h"

#include <glad/gl.h>

namespace sdf3d {
namespace {

bool vec3Equal(const glm::vec3& a, const glm::vec3& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool cameraEqual(const RenderCamera& a, const RenderCamera& b)
{
    return vec3Equal(a.position, b.position)
        && vec3Equal(a.target, b.target)
        && vec3Equal(a.up, b.up)
        && a.fovDegrees == b.fovDegrees;
}

} // namespace

PathTraceAccumulation::~PathTraceAccumulation()
{
    shutdown();
}

void PathTraceAccumulation::init()
{
    if (m_framebuffer == 0) {
        glGenFramebuffers(1, &m_framebuffer);
    }
    if (m_accumulationTexture == 0) {
        glGenTextures(1, &m_accumulationTexture);
    }
}

void PathTraceAccumulation::shutdown()
{
    if (m_accumulationTexture != 0) {
        const GLuint texture = m_accumulationTexture;
        glDeleteTextures(1, &texture);
        m_accumulationTexture = 0;
    }
    if (m_framebuffer != 0) {
        const GLuint framebuffer = m_framebuffer;
        glDeleteFramebuffers(1, &framebuffer);
        m_framebuffer = 0;
    }
    m_lastKey.reset();
    m_sampleCount = 0;
    m_width = 0;
    m_height = 0;
}

bool PathTraceAccumulation::prepareFrame(const PathTraceFrameKey& key)
{
    init();
    if (!resize(key.width, key.height)) {
        return false;
    }

    if (!m_lastKey || !frameKeyMatches(*m_lastKey, key)) {
        m_lastKey = key;
        reset();
    }

    return true;
}

void PathTraceAccumulation::reset()
{
    m_sampleCount = 0;
}

void PathTraceAccumulation::markSampleRendered()
{
    ++m_sampleCount;
}

uint32_t PathTraceAccumulation::sampleCount() const
{
    return m_sampleCount;
}

uint32_t PathTraceAccumulation::accumulationTexture() const
{
    return m_accumulationTexture;
}

bool PathTraceAccumulation::frameKeyMatches(const PathTraceFrameKey& a, const PathTraceFrameKey& b)
{
    return a.width == b.width
        && a.height == b.height
        && cameraEqual(a.camera, b.camera)
        && a.quality == b.quality
        && a.mode == b.mode
        && vec3Equal(a.environmentColor, b.environmentColor)
        && a.sceneRevision == b.sceneRevision
        && a.materialRevision == b.materialRevision
        && a.nodeParamRevision == b.nodeParamRevision;
}

bool PathTraceAccumulation::resize(int width, int height)
{
    const int normalizedWidth = FboRenderer::normalizedDimension(width);
    const int normalizedHeight = FboRenderer::normalizedDimension(height);
    if (m_accumulationTexture == 0 || m_framebuffer == 0) {
        init();
    }
    if (m_width == normalizedWidth && m_height == normalizedHeight) {
        return true;
    }

    glBindTexture(GL_TEXTURE_2D, m_accumulationTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, normalizedWidth, normalizedHeight, 0, GL_RGBA, GL_FLOAT, nullptr);

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_accumulationTexture, 0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (complete) {
        m_width = normalizedWidth;
        m_height = normalizedHeight;
    }
    return complete;
}

} // namespace sdf3d
