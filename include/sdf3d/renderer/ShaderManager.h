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

    /// Returns the latest shader compile/link/reload error, or empty on success.
    const std::string& lastError() const;

    /// Replaces the scene block inside a fragment shader template.
    static std::string injectSceneSource(const std::string& fragmentSource, const std::string& sceneGlsl, std::string& errorLog);

private:
    bool loadProgram(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
    bool loadProgramFromSources(const std::string& vertexSource, const std::string& fragmentSource);
    std::string fragmentSourceWithScene(const std::string& sceneGlsl);

    std::filesystem::path m_vertexShaderPath;
    std::filesystem::path m_fragmentShaderPath;
    unsigned int m_program = 0;
    std::string m_lastError;
};

} // namespace sdf3d
