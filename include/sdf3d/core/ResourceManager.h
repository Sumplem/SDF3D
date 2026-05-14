#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace sdf3d {

using ResourceId = uint64_t;

/// Compile-time FNV-1a hash for ergonomic resource identifiers.
constexpr ResourceId operator""_rid(const char* str, size_t len)
{
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        hash = (hash ^ static_cast<uint64_t>(str[i])) * 1099511628211ULL;
    }
    return hash;
}

/// Lazy resource cache rooted at the runtime assets directory.
class ResourceManager {
public:
    /// Returns the process-wide resource manager instance.
    static ResourceManager& instance();

    /// Initializes fallback GPU resources and records the assets root.
    void init(const std::filesystem::path& assetsRoot);

    /// Unloads cached resources and fallback GPU handles.
    void shutdown();

    /// Lazy-loads a resource or returns nullptr for unsupported M1 resource types.
    template<typename T>
    std::shared_ptr<T> get(ResourceId id, const std::filesystem::path& relativePath)
    {
        static_assert(!std::is_pointer<T>::value, "ResourceManager::get<T> expects an object type");

        const auto existing = m_cache.find(id);
        if (existing != m_cache.end()) {
            return std::static_pointer_cast<T>(existing->second);
        }

        // AGENT: M1 only establishes cache ownership and guaranteed GPU fallbacks.
        // Type-specific loaders are added when their resource classes exist.
        m_errorLog.push_back("No loader registered for resource: " + assetsPath(relativePath).string());
        return nullptr;
    }

    /// Explicitly removes one cached resource.
    void unload(ResourceId id);

    /// Resolves a path relative to the initialized assets root.
    std::filesystem::path assetsPath(const std::filesystem::path& relative = "") const;

    /// Emits pending load errors to the debug console for now.
    void flushErrors();

    /// Returns the in-memory magenta fallback shader program.
    unsigned int fallbackShaderProgram() const;

    /// Returns the in-memory magenta checkerboard fallback texture.
    unsigned int fallbackTexture2D() const;

private:
    ResourceManager() = default;

    void createFallbackShader();
    void createFallbackTexture();
    void destroyFallbacks();
    void logGlError(const std::string& message);

    std::filesystem::path m_assetsRoot;
    std::unordered_map<ResourceId, std::shared_ptr<void>> m_cache;
    std::vector<std::string> m_errorLog;
    unsigned int m_fallbackShaderProgram = 0;
    unsigned int m_fallbackTexture2D = 0;
    bool m_initialized = false;
};

} // namespace sdf3d
