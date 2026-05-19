#pragma once

#include "sdf3d/renderer/UniformUploader.h"

#include <cstdint>
#include <optional>

namespace sdf3d {

struct PathTraceFrameKey {
    int width = 1;
    int height = 1;
    RenderCamera camera;
    RenderQuality quality = RenderQuality::High;
    RenderMode mode = RenderMode::DirectPreview;
    uint64_t sceneRevision = 0;
    uint64_t materialRevision = 0;
    uint64_t nodeParamRevision = 0;
};

/// Owns progressive path-tracing accumulation state and HDR storage.
class PathTraceAccumulation {
public:
    PathTraceAccumulation() = default;
    ~PathTraceAccumulation();

    PathTraceAccumulation(const PathTraceAccumulation&) = delete;
    PathTraceAccumulation& operator=(const PathTraceAccumulation&) = delete;

    void init();
    void shutdown();

    /// Ensures HDR accumulation storage exists and resets when frame inputs changed.
    bool prepareFrame(const PathTraceFrameKey& key);

    /// Clears sample history without releasing GL storage.
    void reset();

    /// Marks one progressive sample rendered.
    void markSampleRendered();

    uint32_t sampleCount() const;
    uint32_t accumulationTexture() const;

    static bool frameKeyMatches(const PathTraceFrameKey& a, const PathTraceFrameKey& b);

private:
    bool resize(int width, int height);

    uint32_t m_framebuffer = 0;
    uint32_t m_accumulationTexture = 0;
    uint32_t m_sampleCount = 0;
    int m_width = 0;
    int m_height = 0;
    std::optional<PathTraceFrameKey> m_lastKey;
};

} // namespace sdf3d
