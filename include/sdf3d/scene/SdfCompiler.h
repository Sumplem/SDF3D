#pragma once

#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/scene/SdfNode.h"
#include "sdf3d/systems/GlslEmitMode.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace sdf3d {

class GraphGroupRegistry;

/// Material data emitted beside generated GLSL for renderer upload.
struct SdfCompiledMaterial {
    SdfMaterial material;
};

/// Runtime node parameters uploaded beside generated GLSL for edit-time fast updates.
struct SdfCompiledNodeParam {
    uint64_t nodeId = 0;
    uint32_t slot = 0;
    std::array<float, 4> data0 = {0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 4> data1 = {0.0f, 0.0f, 0.0f, 0.0f};
};

/// Runtime instanced primitive position uploaded beside generated GLSL.
struct SdfCompiledInstancePosition {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
};

/// Runtime range into the instanced primitive position buffer for one node.
struct SdfCompiledInstanceRange {
    uint64_t nodeId = 0;
    uint32_t first = 0;
    uint32_t count = 0;
};

/// Runtime instanced primitive data gathered from graph payloads.
struct SdfCompiledInstanceData {
    std::vector<SdfCompiledInstancePosition> positions;
    std::vector<SdfCompiledInstanceRange> ranges;
};

/// Result of compiling an SDF node tree into GLSL.
struct SdfCompileResult {
    std::string glsl;
    std::vector<std::string> errors;
    // AGENT: Material list stays beside GLSL so the compiler owns deterministic
    // material ID assignment without coupling SdfNode to renderer uniform layout.
    std::vector<SdfCompiledMaterial> materials;
    std::vector<SdfCompiledNodeParam> nodeParams;
    std::vector<SdfCompiledInstancePosition> instancePositions;
    std::vector<SdfCompiledInstanceRange> instanceRanges;
    std::unordered_map<MaterialId, std::string> materialFunctionByRegistryId;
    bool usesBox = false;
    bool usesCylinder = false;
    bool usesCappedCone = false;
    bool usesSmoothMin = false;
    bool usesRotate = false;
};

/// Emits deterministic GLSL for an SDF expression tree.
class SdfCompiler {
public:
    /// Compiles a tree into self-contained `sceneSDF` and `sceneMaterial` GLSL entry points.
    SdfCompileResult compile(const SdfNodePtr& root, GlslEmitMode mode = GlslEmitMode::Runtime) const;

    /// Compiles a graph output into self-contained SDF GLSL.
    SdfCompileResult compile(const SdfGraph& graph, GlslEmitMode mode = GlslEmitMode::Runtime) const;

    /// Compiles a graph output with resolved group definitions.
    SdfCompileResult compile(const SdfGraph& graph, const GraphGroupRegistry& groups, GlslEmitMode mode = GlslEmitMode::Runtime) const;
};

} // namespace sdf3d
