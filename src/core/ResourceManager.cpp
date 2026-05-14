#include "sdf3d/core/ResourceManager.h"

#include <array>
#include <iostream>
#include <string>

#include <glad/gl.h>

namespace sdf3d {
namespace {

GLuint compileShader(GLenum type, const char* source, std::string& error)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE) {
        return shader;
    }

    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<size_t>(length), '\0');
    if (length > 0) {
        glGetShaderInfoLog(shader, length, nullptr, log.data());
    }

    error = log;
    glDeleteShader(shader);
    return 0;
}

} // namespace

ResourceManager& ResourceManager::instance()
{
    static ResourceManager manager;
    return manager;
}

void ResourceManager::init(const std::filesystem::path& assetsRoot)
{
    if (m_initialized) {
        return;
    }

    m_assetsRoot = assetsRoot;
    createFallbackShader();
    createFallbackTexture();
    m_initialized = true;
}

void ResourceManager::shutdown()
{
    m_cache.clear();
    destroyFallbacks();
    m_errorLog.clear();
    m_assetsRoot.clear();
    m_initialized = false;
}

void ResourceManager::unload(ResourceId id)
{
    m_cache.erase(id);
}

std::filesystem::path ResourceManager::assetsPath(const std::filesystem::path& relative) const
{
    if (relative.empty()) {
        return m_assetsRoot;
    }

    return m_assetsRoot / relative;
}

void ResourceManager::flushErrors()
{
    for (const std::string& error : m_errorLog) {
        std::cerr << "[SDF3D][ResourceManager] " << error << '\n';
    }

    m_errorLog.clear();
}

unsigned int ResourceManager::fallbackShaderProgram() const
{
    return m_fallbackShaderProgram;
}

unsigned int ResourceManager::fallbackTexture2D() const
{
    return m_fallbackTexture2D;
}

void ResourceManager::createFallbackShader()
{
    constexpr const char* vertexSource = R"glsl(
#version 460 core
const vec2 positions[3] = vec2[3](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

void main()
{
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}
)glsl";

    constexpr const char* fragmentSource = R"glsl(
#version 460 core
out vec4 outColor;

void main()
{
    outColor = vec4(1.0, 0.0, 1.0, 1.0);
}
)glsl";

    std::string error;
    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, error);
    if (vertexShader == 0) {
        logGlError("Fallback vertex shader failed: " + error);
        return;
    }

    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, error);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        logGlError("Fallback fragment shader failed: " + error);
        return;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(length), '\0');
        if (length > 0) {
            glGetProgramInfoLog(program, length, nullptr, log.data());
        }

        glDeleteProgram(program);
        logGlError("Fallback shader program failed: " + log);
        return;
    }

    m_fallbackShaderProgram = program;
}

void ResourceManager::createFallbackTexture()
{
    constexpr int size = 8;
    std::array<unsigned char, size * size * 4> pixels{};

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool magenta = ((x / 2) + (y / 2)) % 2 == 0;
            const size_t index = static_cast<size_t>((y * size + x) * 4);
            pixels[index + 0] = magenta ? 255 : 0;
            pixels[index + 1] = 0;
            pixels[index + 2] = magenta ? 255 : 0;
            pixels[index + 3] = 255;
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    m_fallbackTexture2D = texture;
}

void ResourceManager::destroyFallbacks()
{
    if (m_fallbackTexture2D != 0) {
        glDeleteTextures(1, &m_fallbackTexture2D);
        m_fallbackTexture2D = 0;
    }

    if (m_fallbackShaderProgram != 0) {
        glDeleteProgram(m_fallbackShaderProgram);
        m_fallbackShaderProgram = 0;
    }
}

void ResourceManager::logGlError(const std::string& message)
{
    m_errorLog.push_back(message);
}

} // namespace sdf3d
