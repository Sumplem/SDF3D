#include "sdf3d/renderer/FboRenderer.h"

#include <iostream>

#include <glad/gl.h>

namespace sdf3d {

FboRenderer::~FboRenderer()
{
    shutdown();
}

void FboRenderer::init()
{
    if (m_vertexArray == 0) {
        glGenVertexArrays(1, &m_vertexArray);
    }
}

void FboRenderer::shutdown()
{
    if (m_colorTexture != 0) {
        glDeleteTextures(1, &m_colorTexture);
        m_colorTexture = 0;
    }

    if (m_framebuffer != 0) {
        glDeleteFramebuffers(1, &m_framebuffer);
        m_framebuffer = 0;
    }

    if (m_vertexArray != 0) {
        glDeleteVertexArrays(1, &m_vertexArray);
        m_vertexArray = 0;
    }
}

void FboRenderer::resize(int width, int height)
{
    const int newWidth = normalizedDimension(width);
    const int newHeight = normalizedDimension(height);
    if (newWidth == m_width && newHeight == m_height && m_framebuffer != 0) {
        return;
    }

    m_width = newWidth;
    m_height = newHeight;
    resizeFramebuffer();
}

bool FboRenderer::begin()
{
    if (m_framebuffer == 0) {
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.08f, 0.09f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return true;
}

void FboRenderer::drawFullscreenTriangle()
{
    glBindVertexArray(m_vertexArray);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void FboRenderer::end()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

unsigned int FboRenderer::outputTexture() const
{
    return m_colorTexture;
}

int FboRenderer::width() const
{
    return m_width;
}

int FboRenderer::height() const
{
    return m_height;
}

int FboRenderer::normalizedDimension(int value)
{
    return value > 0 ? value : 1;
}

void FboRenderer::resizeFramebuffer()
{
    if (m_framebuffer == 0) {
        glGenFramebuffers(1, &m_framebuffer);
    }

    if (m_colorTexture == 0) {
        glGenTextures(1, &m_colorTexture);
    }

    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

    // AGENT: The raymarch pass writes color only; depth storage is unnecessary
    // until viewport mixes rasterized overlays or gizmos into the same FBO.
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[SDF3D][FboRenderer] Viewport framebuffer is incomplete: " << status << '\n';
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace sdf3d
