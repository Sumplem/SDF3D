#include "sdf3d/renderer/ShaderManager.h"

#include <fstream>
#include <iostream>
#include <sstream>

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

unsigned int compileShader(unsigned int type, const std::string& source, const char* label, std::string& errorLog)
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

    errorLog = std::string("Failed to compile ") + label + " shader:\n" + log;
    std::cerr << "[SDF3D][ShaderManager] " << errorLog << '\n';
    glDeleteShader(shader);
    return 0;
}

} // namespace

ShaderManager::~ShaderManager()
{
    shutdown();
}

bool ShaderManager::init(const std::filesystem::path& shaderRoot)
{
    m_vertexShaderPath = shaderRoot / "raymarch.vert";
    m_fragmentShaderPath = shaderRoot / "raymarch.frag";
    return loadProgram(m_vertexShaderPath, m_fragmentShaderPath);
}

void ShaderManager::shutdown()
{
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

bool ShaderManager::reloadScene(const std::string& sceneGlsl)
{
    const std::string vertexSource = readTextFile(m_vertexShaderPath);
    const std::string fragmentSource = fragmentSourceWithScene(sceneGlsl);
    if (vertexSource.empty() || fragmentSource.empty()) {
        m_lastError = "Failed to reload scene shader sources.";
        std::cerr << "[SDF3D][ShaderManager] " << m_lastError << '\n';
        return false;
    }

    return loadProgramFromSources(vertexSource, fragmentSource);
}

unsigned int ShaderManager::program() const
{
    return m_program;
}

const std::string& ShaderManager::lastError() const
{
    return m_lastError;
}

std::string ShaderManager::injectSceneSource(const std::string& fragmentSource, const std::string& sceneGlsl, std::string& errorLog)
{
    constexpr const char* beginMarker = "// SDF3D_SCENE_BEGIN";
    constexpr const char* endMarker = "// SDF3D_SCENE_END";

    const size_t begin = fragmentSource.find(beginMarker);
    const size_t end = fragmentSource.find(endMarker);
    if (begin == std::string::npos || end == std::string::npos || end <= begin) {
        errorLog = "Scene shader injection markers were not found.";
        return {};
    }

    const size_t blockStart = fragmentSource.find('\n', begin);
    if (blockStart == std::string::npos) {
        errorLog = "Scene shader begin marker has no trailing newline.";
        return {};
    }

    std::string injected = fragmentSource;
    // AGENT: Marker comments stay in shader source so generated shader logs map
    // back to the template file after scene GLSL injection.
    injected.replace(blockStart + 1, end - blockStart - 1, sceneGlsl);
    errorLog.clear();
    return injected;
}

bool ShaderManager::loadProgram(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
{
    const std::string vertexSource = readTextFile(vertexPath);
    const std::string fragmentSource = readTextFile(fragmentPath);
    if (vertexSource.empty() || fragmentSource.empty()) {
        m_lastError = "Failed to read shader assets:\n  " + vertexPath.string() + "\n  " + fragmentPath.string();
        std::cerr << "[SDF3D][ShaderManager] " << m_lastError << '\n';
        return false;
    }

    return loadProgramFromSources(vertexSource, fragmentSource);
}

bool ShaderManager::loadProgramFromSources(const std::string& vertexSource, const std::string& fragmentSource)
{
    std::string errorLog;
    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, "vertex", errorLog);
    if (vertexShader == 0) {
        m_lastError = errorLog;
        return false;
    }

    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "fragment", errorLog);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        m_lastError = errorLog;
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

        m_lastError = "Failed to link shader program:\n" + log;
        std::cerr << "[SDF3D][ShaderManager] " << m_lastError << '\n';
        glDeleteProgram(program);
        return false;
    }

    if (m_program != 0) {
        glDeleteProgram(m_program);
    }

    m_program = program;
    m_lastError.clear();
    return true;
}

std::string ShaderManager::fragmentSourceWithScene(const std::string& sceneGlsl)
{
    const std::string fragmentSource = readTextFile(m_fragmentShaderPath);
    if (fragmentSource.empty()) {
        return {};
    }

    std::string errorLog;
    std::string injected = injectSceneSource(fragmentSource, sceneGlsl, errorLog);
    if (injected.empty() && !errorLog.empty()) {
        std::cerr << "[SDF3D][ShaderManager] " << errorLog << '\n';
    }
    return injected;
}

} // namespace sdf3d
