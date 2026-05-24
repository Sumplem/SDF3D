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
    m_editFragmentShaderPath = shaderRoot / "raymarch_edit.frag";
    m_sceneFragmentShaderPath = shaderRoot / "raymarch.frag";
    m_pathTraceFragmentShaderPath = shaderRoot / "raymarch_pathtrace.frag";
    return loadProgram(m_vertexShaderPath, m_editFragmentShaderPath, m_editProgram);
}

void ShaderManager::shutdown()
{
    if (m_editProgram != 0) {
        glDeleteProgram(m_editProgram);
        m_editProgram = 0;
    }
    if (m_sceneProgram != 0) {
        glDeleteProgram(m_sceneProgram);
        m_sceneProgram = 0;
    }
    if (m_pathTraceProgram != 0) {
        glDeleteProgram(m_pathTraceProgram);
        m_pathTraceProgram = 0;
    }
}

bool ShaderManager::reloadScene(const std::string& sceneGlsl)
{
    const std::string vertexSource = readTextFile(m_vertexShaderPath);
    const std::string fragmentSource = fragmentSourceWithScene(m_editFragmentShaderPath, sceneGlsl);
    if (vertexSource.empty() || fragmentSource.empty()) {
        m_lastError = "Failed to reload scene shader sources.";
        std::cerr << "[SDF3D][ShaderManager] " << m_lastError << '\n';
        return false;
    }

    if (!loadProgramFromSources(vertexSource, fragmentSource, m_editProgram)) {
        return false;
    }

    m_lastSceneGlsl = sceneGlsl;
    if (m_sceneProgram != 0) {
        const std::string sceneFragmentSource = fragmentSourceWithScene(m_sceneFragmentShaderPath, sceneGlsl);
        if (sceneFragmentSource.empty()) {
            m_lastError = "Failed to reload scene-only shader source.";
            std::cerr << "[SDF3D][ShaderManager] " << m_lastError << '\n';
            return false;
        }
        if (!loadProgramFromSources(vertexSource, sceneFragmentSource, m_sceneProgram)) {
            return false;
        }
    }

    if (m_pathTraceProgram != 0) {
        const std::string pathTraceFragmentSource = fragmentSourceWithScene(m_pathTraceFragmentShaderPath, sceneGlsl);
        if (pathTraceFragmentSource.empty()) {
            m_lastError = "Failed to reload path-tracing shader source.";
            std::cerr << "[SDF3D][ShaderManager] " << m_lastError << '\n';
            return false;
        }
        return loadProgramFromSources(vertexSource, pathTraceFragmentSource, m_pathTraceProgram);
    }

    return true;
}

unsigned int ShaderManager::program() const
{
    return m_editProgram;
}

unsigned int ShaderManager::sceneProgram()
{
    if (m_sceneProgram != 0) {
        return m_sceneProgram;
    }

    const std::string vertexSource = readTextFile(m_vertexShaderPath);
    const std::string fragmentSource = m_lastSceneGlsl.empty()
        ? readTextFile(m_sceneFragmentShaderPath)
        : fragmentSourceWithScene(m_sceneFragmentShaderPath, m_lastSceneGlsl);
    if (vertexSource.empty() || fragmentSource.empty()) {
        m_lastError = "Failed to load scene-only shader sources.";
        std::cerr << "[SDF3D][ShaderManager] " << m_lastError << '\n';
        return 0;
    }

    return loadProgramFromSources(vertexSource, fragmentSource, m_sceneProgram) ? m_sceneProgram : 0;
}

unsigned int ShaderManager::pathTraceProgram()
{
    if (m_pathTraceProgram != 0) {
        return m_pathTraceProgram;
    }

    const std::string vertexSource = readTextFile(m_vertexShaderPath);
    const std::string fragmentSource = m_lastSceneGlsl.empty()
        ? readTextFile(m_pathTraceFragmentShaderPath)
        : fragmentSourceWithScene(m_pathTraceFragmentShaderPath, m_lastSceneGlsl);
    if (vertexSource.empty() || fragmentSource.empty()) {
        m_lastError = "Failed to load path-tracing shader sources.";
        std::cerr << "[SDF3D][ShaderManager] " << m_lastError << '\n';
        return 0;
    }

    return loadProgramFromSources(vertexSource, fragmentSource, m_pathTraceProgram) ? m_pathTraceProgram : 0;
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

bool ShaderManager::writeInjectedFragmentSource(
    const std::filesystem::path& fragmentTemplatePath,
    const std::string& sceneGlsl,
    const std::filesystem::path& outputPath,
    std::string& errorLog)
{
    const std::string fragmentSource = readTextFile(fragmentTemplatePath);
    if (fragmentSource.empty()) {
        errorLog = "Failed to read shader template: " + fragmentTemplatePath.string();
        return false;
    }

    const std::string injected = injectSceneSource(fragmentSource, sceneGlsl, errorLog);
    if (injected.empty()) {
        return false;
    }

    const std::filesystem::path parent = outputPath.parent_path();
    if (!parent.empty()) {
        std::error_code directoryError;
        std::filesystem::create_directories(parent, directoryError);
        if (directoryError) {
            errorLog = "Failed to create shader export directory: " + parent.string();
            return false;
        }
    }

    std::ofstream output(outputPath);
    if (!output) {
        errorLog = "Failed to open shader export path: " + outputPath.string();
        return false;
    }

    output << injected;
    if (!output) {
        errorLog = "Failed to write shader export path: " + outputPath.string();
        return false;
    }

    errorLog.clear();
    return true;
}

bool ShaderManager::loadProgram(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, unsigned int& program)
{
    const std::string vertexSource = readTextFile(vertexPath);
    const std::string fragmentSource = readTextFile(fragmentPath);
    if (vertexSource.empty() || fragmentSource.empty()) {
        m_lastError = "Failed to read shader assets:\n  " + vertexPath.string() + "\n  " + fragmentPath.string();
        std::cerr << "[SDF3D][ShaderManager] " << m_lastError << '\n';
        return false;
    }

    return loadProgramFromSources(vertexSource, fragmentSource, program);
}

bool ShaderManager::loadProgramFromSources(const std::string& vertexSource, const std::string& fragmentSource, unsigned int& program)
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

    const GLuint linkedProgram = glCreateProgram();
    glAttachShader(linkedProgram, vertexShader);
    glAttachShader(linkedProgram, fragmentShader);
    glLinkProgram(linkedProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint success = GL_FALSE;
    glGetProgramiv(linkedProgram, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(linkedProgram, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(length), '\0');
        if (length > 0) {
            glGetProgramInfoLog(linkedProgram, length, nullptr, log.data());
        }

        m_lastError = "Failed to link shader program:\n" + log;
        std::cerr << "[SDF3D][ShaderManager] " << m_lastError << '\n';
        glDeleteProgram(linkedProgram);
        return false;
    }

    if (program != 0) {
        glDeleteProgram(program);
    }

    program = linkedProgram;
    m_lastError.clear();
    return true;
}

std::string ShaderManager::fragmentSourceWithScene(const std::filesystem::path& fragmentPath, const std::string& sceneGlsl)
{
    const std::string fragmentSource = readTextFile(fragmentPath);
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
