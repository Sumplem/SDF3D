#include "sdf3d/renderer/Renderer.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include <glad/gl.h>

namespace sdf3d {
namespace {

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file) {
        return {};
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

unsigned int compileShader(unsigned int type, const std::string& source, const char* label)
{
    const GLuint shader = glCreateShader(type);
    const char* sourcePtr = source.c_str();
    glShaderSource(shader, 1, &sourcePtr, nullptr);
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

    std::cerr << "[SDF3D][Renderer] Failed to compile " << label << " shader:\n" << log << '\n';
    glDeleteShader(shader);
    return 0;
}

void setUniformVec3(GLuint program, const std::string& name, const glm::vec3& value)
{
    const GLint location = glGetUniformLocation(program, name.c_str());
    if (location >= 0) {
        glUniform3fv(location, 1, &value.x);
    }
}

void setUniformFloat(GLuint program, const std::string& name, float value)
{
    const GLint location = glGetUniformLocation(program, name.c_str());
    if (location >= 0) {
        glUniform1f(location, value);
    }
}

} // namespace

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init(const std::filesystem::path& shaderRoot)
{
    glGenVertexArrays(1, &m_vertexArray);
    m_vertexShaderPath = shaderRoot / "raymarch.vert";
    m_fragmentShaderPath = shaderRoot / "raymarch.frag";

    // AGENT: Shader files stay in assets so M2 exercises the same runtime asset
    // path used by later hot-reload instead of embedding throwaway GLSL strings.
    return loadProgram(m_vertexShaderPath, m_fragmentShaderPath);
}

void Renderer::shutdown()
{
    if (m_colorTexture != 0) {
        glDeleteTextures(1, &m_colorTexture);
        m_colorTexture = 0;
    }

    if (m_framebuffer != 0) {
        glDeleteFramebuffers(1, &m_framebuffer);
        m_framebuffer = 0;
    }

    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }

    if (m_vertexArray != 0) {
        glDeleteVertexArrays(1, &m_vertexArray);
        m_vertexArray = 0;
    }
}

void Renderer::resize(int width, int height)
{
    const int newWidth = width > 0 ? width : 1;
    const int newHeight = height > 0 ? height : 1;
    if (newWidth == m_width && newHeight == m_height && m_framebuffer != 0) {
        return;
    }

    m_width = newWidth;
    m_height = newHeight;
    resizeFramebuffer();
}

void Renderer::render(const RenderCamera& camera)
{
    if (m_program == 0 || m_framebuffer == 0) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.08f, 0.09f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(m_program);

    glUniform2f(glGetUniformLocation(m_program, "uResolution"), static_cast<float>(m_width), static_cast<float>(m_height));
    glUniform3fv(glGetUniformLocation(m_program, "uCameraPosition"), 1, &camera.position.x);
    glUniform3fv(glGetUniformLocation(m_program, "uCameraTarget"), 1, &camera.target.x);
    glUniform3fv(glGetUniformLocation(m_program, "uCameraUp"), 1, &camera.up.x);
    glUniform1f(glGetUniformLocation(m_program, "uFovDegrees"), camera.fovDegrees);

    constexpr size_t maxShaderMaterials = 64;
    const size_t materialCount = std::min(m_materials.size(), maxShaderMaterials);
    glUniform1i(glGetUniformLocation(m_program, "uMaterialCount"), static_cast<GLint>(materialCount));
    for (size_t i = 0; i < materialCount; ++i) {
        const SdfMaterial& material = m_materials[i].material;
        const std::string index = std::to_string(i);
        // AGENT: Uniform arrays keep the first material path simple and avoid a
        // renderer-side UBO layout contract until material count becomes large.
        setUniformVec3(m_program, "uMaterialAlbedo[" + index + "]", material.albedo);
        setUniformFloat(m_program, "uMaterialRoughness[" + index + "]", material.roughness);
        setUniformFloat(m_program, "uMaterialMetallic[" + index + "]", material.metallic);
        setUniformFloat(m_program, "uMaterialEmission[" + index + "]", material.emission);
    }

    glBindVertexArray(m_vertexArray);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Renderer::reloadScene(const std::string& sceneGlsl)
{
    const std::string vertexSource = readTextFile(m_vertexShaderPath);
    const std::string fragmentSource = fragmentSourceWithScene(sceneGlsl);
    if (vertexSource.empty() || fragmentSource.empty()) {
        std::cerr << "[SDF3D][Renderer] Failed to reload scene shader sources.\n";
        return false;
    }

    return loadProgramFromSources(vertexSource, fragmentSource);
}

void Renderer::setMaterials(std::vector<SdfCompiledMaterial> materials)
{
    m_materials = std::move(materials);
}

unsigned int Renderer::outputTexture() const
{
    return m_colorTexture;
}

bool Renderer::loadProgram(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
{
    const std::string vertexSource = readTextFile(vertexPath);
    const std::string fragmentSource = readTextFile(fragmentPath);
    if (vertexSource.empty() || fragmentSource.empty()) {
        std::cerr << "[SDF3D][Renderer] Failed to read shader assets:\n"
                  << "  " << vertexPath << '\n'
                  << "  " << fragmentPath << '\n';
        return false;
    }

    return loadProgramFromSources(vertexSource, fragmentSource);
}

bool Renderer::loadProgramFromSources(const std::string& vertexSource, const std::string& fragmentSource)
{
    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, "vertex");
    if (vertexShader == 0) {
        return false;
    }

    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "fragment");
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
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

        std::cerr << "[SDF3D][Renderer] Failed to link shader program:\n" << log << '\n';
        glDeleteProgram(program);
        return false;
    }

    if (m_program != 0) {
        glDeleteProgram(m_program);
    }

    m_program = program;
    return true;
}

std::string Renderer::fragmentSourceWithScene(const std::string& sceneGlsl) const
{
    std::string fragmentSource = readTextFile(m_fragmentShaderPath);
    if (fragmentSource.empty()) {
        return {};
    }

    constexpr const char* beginMarker = "// SDF3D_SCENE_BEGIN";
    constexpr const char* endMarker = "// SDF3D_SCENE_END";

    const size_t begin = fragmentSource.find(beginMarker);
    const size_t end = fragmentSource.find(endMarker);
    if (begin == std::string::npos || end == std::string::npos || end <= begin) {
        std::cerr << "[SDF3D][Renderer] Scene shader injection markers were not found.\n";
        return {};
    }

    const size_t blockStart = fragmentSource.find('\n', begin);
    if (blockStart == std::string::npos) {
        return {};
    }

    // AGENT: The marker comments remain in the shader source, which makes
    // generated shader logs easier to map back to the template file.
    fragmentSource.replace(blockStart + 1, end - blockStart - 1, sceneGlsl);
    return fragmentSource;
}

void Renderer::resizeFramebuffer()
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
    // until the viewport mixes rasterized overlays or gizmos into the same FBO.
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[SDF3D][Renderer] Viewport framebuffer is incomplete: " << status << '\n';
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace sdf3d
