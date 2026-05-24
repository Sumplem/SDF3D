#pragma once

#include <filesystem>
#include <string>

namespace sdf3d {

/// Owns OpenGL shader program compile, link, and scene hot-reload.
class ShaderManager {
public:
    ShaderManager() = default;
    ~ShaderManager();

    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    /// Compiles shader assets from a shader directory.
    bool init(const std::filesystem::path& shaderRoot);

    /// Releases the linked OpenGL program.
    void shutdown();

    /// Rebuilds the fragment shader after replacing the scene injection block.
    bool reloadScene(const std::string& sceneGlsl);

    /// Returns the active linked OpenGL program.
    unsigned int program() const;

    /// Returns the scene-only linked OpenGL program, loading it on demand.
    unsigned int sceneProgram();

    /// Returns the path-tracing linked OpenGL program, loading it on demand.
    unsigned int pathTraceProgram();

    /// Returns the latest shader compile/link/reload error, or empty on success.
    const std::string& lastError() const;

    /// Replaces the scene block inside a fragment shader template.
    static std::string injectSceneSource(const std::string& fragmentSource, const std::string& sceneGlsl, std::string& errorLog);

    /// Writes one fragment shader template after injecting compiled scene GLSL.
    static bool writeInjectedFragmentSource(
        const std::filesystem::path& fragmentTemplatePath,
        const std::string& sceneGlsl,
        const std::filesystem::path& outputPath,
        std::string& errorLog);

private:
    bool loadProgram(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, unsigned int& program);
    bool loadProgramFromSources(const std::string& vertexSource, const std::string& fragmentSource, unsigned int& program);
    std::string fragmentSourceWithScene(const std::filesystem::path& fragmentPath, const std::string& sceneGlsl);

    std::filesystem::path m_vertexShaderPath;
    std::filesystem::path m_editFragmentShaderPath;
    std::filesystem::path m_sceneFragmentShaderPath;
    std::filesystem::path m_pathTraceFragmentShaderPath;
    unsigned int m_editProgram = 0;
    unsigned int m_sceneProgram = 0;
    unsigned int m_pathTraceProgram = 0;
    std::string m_lastSceneGlsl;
    std::string m_lastError;
};

} // namespace sdf3d
