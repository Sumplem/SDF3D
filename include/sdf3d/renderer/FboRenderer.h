#pragma once

namespace sdf3d {

/// Owns viewport FBO, output texture, and fullscreen triangle draw.
class FboRenderer {
public:
    FboRenderer() = default;
    ~FboRenderer();

    FboRenderer(const FboRenderer&) = delete;
    FboRenderer& operator=(const FboRenderer&) = delete;

    /// Creates OpenGL objects needed for fullscreen rendering.
    void init();

    /// Releases OpenGL objects.
    void shutdown();

    /// Updates output viewport dimensions.
    void resize(int width, int height);

    /// Binds and clears the output framebuffer.
    bool begin();

    /// Draws one fullscreen triangle.
    void drawFullscreenTriangle();

    /// Restores default framebuffer binding.
    void end();

    /// Returns the color texture containing latest output.
    unsigned int outputTexture() const;

    /// Returns the integer node-id texture containing latest edit pick output.
    unsigned int nodeIdTexture() const;

    /// Reads one node id from the integer pick attachment.
    int readNodeIdPixel(int x, int y) const;

    /// Returns current output width.
    int width() const;

    /// Returns current output height.
    int height() const;

    /// Clamps invalid framebuffer dimensions to valid GL texture dimensions.
    static int normalizedDimension(int value);

private:
    void resizeFramebuffer();

    unsigned int m_vertexArray = 0;
    unsigned int m_framebuffer = 0;
    unsigned int m_colorTexture = 0;
    unsigned int m_nodeIdTexture = 0;
    int m_width = 1;
    int m_height = 1;
};

} // namespace sdf3d
