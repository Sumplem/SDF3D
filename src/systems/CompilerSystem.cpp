#include "sdf3d/systems/CompilerSystem.h"

#include "sdf3d/scene/SdfGraphCompiler.h"
#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/systems/GlslEmitter.h"
#include "sdf3d/systems/MaterialGraphCompiler.h"
#include "sdf3d/systems/MaterialSystem.h"
#include "sdf3d/systems/GraphSystem.h"

#include "glsl_emitter/GlslEmitterMath.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sdf3d {
namespace {

void collectDescendantHelperIds(
    const SdfNodePtr& node,
    const GlslSdfHelperBlock& sdfHelpers,
    std::vector<uint64_t>& ids,
    std::unordered_set<uint64_t>& seenIds)
{
    if (!node) {
        return;
    }
    const auto it = sdfHelpers.nodeIdByNode.find(node.get());
    if (it != sdfHelpers.nodeIdByNode.end() && seenIds.insert(it->second).second) {
        ids.push_back(it->second);
    }
    for (const SdfNodePtr& child : node->children) {
        collectDescendantHelperIds(child, sdfHelpers, ids, seenIds);
    }
}

void emitSceneNodeContainsCase(
    std::ostringstream& glsl,
    const SdfNodePtr& node,
    const GlslSdfHelperBlock& sdfHelpers,
    std::unordered_set<uint64_t>& emittedCases)
{
    if (!node) {
        return;
    }

    const auto nodeIt = sdfHelpers.nodeIdByNode.find(node.get());
    if (nodeIt != sdfHelpers.nodeIdByNode.end() && emittedCases.insert(nodeIt->second).second) {
        std::vector<uint64_t> ids;
        std::unordered_set<uint64_t> seenIds;
        collectDescendantHelperIds(node, sdfHelpers, ids, seenIds);
        glsl << "    case " << nodeIt->second << ": return ";
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (i != 0) {
                glsl << " || ";
            }
            glsl << "nodeId == " << ids[i];
        }
        glsl << ";\n";
    }

    for (const SdfNodePtr& child : node->children) {
        emitSceneNodeContainsCase(glsl, child, sdfHelpers, emittedCases);
    }
}

float parameterOr(const SdfNode& node, const std::string& key, float fallback)
{
    const auto it = node.parameters.find(key);
    return it == node.parameters.end() ? fallback : it->second;
}

using InstanceRangeByNodeId = std::unordered_map<uint64_t, SdfCompiledInstanceRange>;

SdfCompiledInstanceRange instanceRangeFor(uint64_t nodeId, const InstanceRangeByNodeId& ranges)
{
    const auto it = ranges.find(nodeId);
    if (it == ranges.end()) {
        return {};
    }
    return it->second;
}

bool runtimeNodeParamForTreeNode(const SdfNodePtr& node, SdfCompiledNodeParam& param, const InstanceRangeByNodeId& instanceRanges)
{
    if (!node || node->stableId == 0) {
        return false;
    }

    param.nodeId = node->stableId;
    param.data0 = {0.0f, 0.0f, 0.0f, 0.0f};

    switch (node->type) {
    case SdfNodeType::SphereInstances: {
        const SdfCompiledInstanceRange range = instanceRangeFor(param.nodeId, instanceRanges);
        param.data0 = {
            parameterOr(*node, "radius", 1.0f),
            static_cast<float>(range.first),
            static_cast<float>(range.count),
            0.0f,
        };
        return true;
    }
    case SdfNodeType::Rotate: {
        const glm::vec4 q = rotationQuaternionForNode(*node);
        param.data0 = {q.x, q.y, q.z, q.w};
        return true;
    }
    case SdfNodeType::Scale: {
        const float uniformScale = parameterOr(*node, "scale", 1.0f);
        const float x = std::max(parameterOr(*node, "x", uniformScale), 0.0001f);
        const float y = std::max(parameterOr(*node, "y", uniformScale), 0.0001f);
        const float z = std::max(parameterOr(*node, "z", uniformScale), 0.0001f);
        param.data0 = {x, y, z, std::min({x, y, z})};
        return true;
    }
    default:
        break;
    }

    const SdfNodeDefinition* definition = sdfNodeDefinition(node->type);
    if (definition == nullptr) {
        return false;
    }

    std::size_t packedCount = 0;
    for (const SdfParameterDefinition& parameter : definition->parameters) {
        if (parameter.type != SdfParameterType::Float) {
            continue;
        }
        if (packedCount >= param.data0.size()) {
            break;
        }
        param.data0[packedCount] = parameterOr(*node, parameter.name, parameter.defaultValue);
        ++packedCount;
    }

    return packedCount > 0;
}

void collectTreeNodeParams(const SdfNodePtr& node, std::vector<SdfCompiledNodeParam>& params, std::unordered_set<uint64_t>& packedIds, const InstanceRangeByNodeId& instanceRanges)
{
    if (!node) {
        return;
    }

    SdfCompiledNodeParam param;
    if (runtimeNodeParamForTreeNode(node, param, instanceRanges) && packedIds.insert(param.nodeId).second) {
        params.push_back(param);
    }

    for (const SdfNodePtr& child : node->children) {
        collectTreeNodeParams(child, params, packedIds, instanceRanges);
    }
}

void assignNodeParamSlots(std::vector<SdfCompiledNodeParam>& params)
{
    std::sort(params.begin(), params.end(), [](const SdfCompiledNodeParam& left, const SdfCompiledNodeParam& right) {
        return left.nodeId < right.nodeId;
    });
    for (std::size_t i = 0; i < params.size(); ++i) {
        params[i].slot = static_cast<uint32_t>(i);
    }
}

void collectTreeInstanceData(const SdfNodePtr& node, SdfCompiledInstanceData& instances, std::unordered_set<uint64_t>& packedIds)
{
    if (!node) {
        return;
    }

    if (node->type == SdfNodeType::SphereInstances && node->stableId != 0 && packedIds.insert(node->stableId).second) {
        const uint32_t first = static_cast<uint32_t>(instances.positions.size());
        for (const glm::vec3& position : node->instancePositions) {
            instances.positions.push_back({position});
        }
        instances.ranges.push_back({node->stableId, first, static_cast<uint32_t>(node->instancePositions.size())});
    }

    for (const SdfNodePtr& child : node->children) {
        collectTreeInstanceData(child, instances, packedIds);
    }
}

SdfCompiledInstanceData collectTreeInstanceData(const SdfNodePtr& root)
{
    SdfCompiledInstanceData instances;
    std::unordered_set<uint64_t> packedIds;
    collectTreeInstanceData(root, instances, packedIds);
    std::sort(instances.ranges.begin(), instances.ranges.end(), [](const SdfCompiledInstanceRange& left, const SdfCompiledInstanceRange& right) {
        return left.nodeId < right.nodeId;
    });
    return instances;
}

InstanceRangeByNodeId instanceRangeMap(const SdfCompiledInstanceData& instances)
{
    InstanceRangeByNodeId ranges;
    for (const SdfCompiledInstanceRange& range : instances.ranges) {
        ranges.emplace(range.nodeId, range);
    }
    return ranges;
}

std::vector<SdfCompiledNodeParam> collectTreeNodeParams(const SdfNodePtr& root, const SdfCompiledInstanceData& instances)
{
    std::vector<SdfCompiledNodeParam> params;
    std::unordered_set<uint64_t> packedIds;
    collectTreeNodeParams(root, params, packedIds, instanceRangeMap(instances));
    assignNodeParamSlots(params);
    return params;
}

std::vector<MaterialDefinition> materialDefinitionsForGraph(const SdfGraph& graph)
{
    std::vector<MaterialDefinition> materials;
    for (const MaterialDefinition& material : graph.materials().materials()) {
        materials.push_back(material);
    }
    return materials;
}

void appendGroupMaterialDefinitions(const GraphGroupRegistry& groups, std::vector<MaterialDefinition>& materials)
{
    for (const GraphGroupDefinition& definition : groups.definitions()) {
        for (const MaterialDefinition& material : definition.subgraph.materials().materials()) {
            materials.push_back(material);
        }
    }
}

std::vector<MaterialDefinition> deduplicateMaterialDefinitions(std::vector<MaterialDefinition> materials)
{
    std::vector<MaterialDefinition> unique;
    std::unordered_set<MaterialId> seen;
    for (MaterialDefinition& material : materials) {
        if (material.id != 0 && seen.insert(material.id).second) {
            unique.push_back(std::move(material));
        }
    }
    return unique;
}

} // namespace

SdfCompileResult CompilerSystem::compile(const SdfGraph& graph, GlslEmitMode mode) const
{
    const SdfGraphLowerResult lowered = lowerSdfGraphToTree(graph);
    std::vector<SdfCompiledNodeParam> nodeParams;
    SdfCompiledInstanceData instances;
    if (mode == GlslEmitMode::Runtime) {
        instances = GraphSystem::collectInstanceData(graph);
        nodeParams = GraphSystem::collectNodeParams(graph);
    }
    SdfCompileResult result = compileTree(lowered.root, mode, std::move(nodeParams), materialDefinitionsForGraph(graph));
    result.instancePositions = std::move(instances.positions);
    result.instanceRanges = std::move(instances.ranges);
    result.errors.insert(result.errors.begin(), lowered.errors.begin(), lowered.errors.end());
    return result;
}

SdfCompileResult CompilerSystem::compile(const SdfGraph& graph, const GraphGroupRegistry& groups, GlslEmitMode mode) const
{
    const SdfGraphLowerResult lowered = lowerSdfGraphToTree(graph, groups);
    std::vector<SdfCompiledNodeParam> nodeParams;
    SdfCompiledInstanceData instances;
    if (mode == GlslEmitMode::Runtime) {
        instances = GraphSystem::collectInstanceData(graph, groups);
        nodeParams = GraphSystem::collectNodeParams(graph, groups);
    }
    std::vector<MaterialDefinition> materials = materialDefinitionsForGraph(graph);
    appendGroupMaterialDefinitions(groups, materials);
    SdfCompileResult result = compileTree(lowered.root, mode, std::move(nodeParams), deduplicateMaterialDefinitions(std::move(materials)));
    result.instancePositions = std::move(instances.positions);
    result.instanceRanges = std::move(instances.ranges);
    result.errors.insert(result.errors.begin(), lowered.errors.begin(), lowered.errors.end());
    return result;
}

SdfCompileResult CompilerSystem::compile(const SdfNodePtr& root, GlslEmitMode mode) const
{
    SdfCompiledInstanceData instances;
    std::vector<SdfCompiledNodeParam> nodeParams;
    if (mode == GlslEmitMode::Runtime) {
        instances = collectTreeInstanceData(root);
        nodeParams = collectTreeNodeParams(root, instances);
    }
    SdfCompileResult result = compileTree(root, mode, std::move(nodeParams));
    result.instancePositions = std::move(instances.positions);
    result.instanceRanges = std::move(instances.ranges);
    return result;
}

SdfCompileResult CompilerSystem::compileTree(
    const SdfNodePtr& root,
    GlslEmitMode mode,
    std::vector<SdfCompiledNodeParam> nodeParams,
    std::vector<MaterialDefinition> materialDefinitions) const
{
    SdfCompileResult result;
    result.nodeParams = std::move(nodeParams);
    MaterialGraphCompiler materialGraphCompiler;
    std::ostringstream materialGraphFunctions;
    for (const MaterialDefinition& material : materialDefinitions) {
        materialGraphFunctions << materialGraphCompiler.emitMaterialFunction(material, result);
    }

    if (!root) {
        result.errors.push_back("Cannot compile an empty SDF tree.");
        // AGENT: Empty scenes still emit both runtime entry points so the shader
        // template stays valid before the user adds geometry.
        result.glsl =
            "vec2 sceneSDFWithId(vec3 p)\n"
            "{\n"
            "    return vec2(1e6, -1.0);\n"
            "}\n\n"
            "float sceneSDF(vec3 p)\n"
            "{\n"
            "    return sceneSDFWithId(p).x;\n"
            "}\n\n"
            "SdfMaterialSample sceneMaterial(vec3 p)\n"
            "{\n"
            "    return sampleMaterial(0, p);\n"
            "}\n\n"
            "float sceneNodeSDF(int nodeId, vec3 p)\n"
            "{\n"
            "    return 1e6;\n"
            "}\n\n"
            "int scenePickId(vec3 p)\n"
            "{\n"
            "    return int(sceneSDFWithId(p).y);\n"
            "}\n\n"
            "bool sceneNodeContains(int nodeId, int visibleNodeId)\n"
            "{\n"
            "    return nodeId == visibleNodeId;\n"
            "}\n";
        return result;
    }

    const GlslEmitter emitter(mode);
    const GlslSdfHelperBlock sdfHelpers = emitter.emitSdfHelpers(root, result);
    const MaterialSystem materialSystem;
    const std::string materialBody = emitter.emitSceneMaterialBody(root, "p", result, sdfHelpers, materialSystem);
    const std::string sdfWithIdBody = emitter.emitSceneSdfWithIdBody(root, "p", result, sdfHelpers);

    std::ostringstream glsl;
    if (mode == GlslEmitMode::Runtime) {
        glsl << "struct SdfNodeParam\n";
        glsl << "{\n";
        glsl << "    vec4 data0;\n";
        glsl << "};\n\n";
        glsl << "layout(std430, binding = 1) readonly buffer NodeParamBuffer\n";
        glsl << "{\n";
        glsl << "    SdfNodeParam uNodeParams[];\n";
        glsl << "};\n\n";
        glsl << "uniform int uNodeParamCount;\n\n";
        glsl << "layout(std430, binding = 2) readonly buffer InstancePositionBuffer\n";
        glsl << "{\n";
        glsl << "    vec4 uInstancePositions[];\n";
        glsl << "};\n\n";
        glsl << "uniform int uInstancePositionCount;\n\n";
        glsl << "float sdf3d_sphere_instances(vec3 p, float radius, int first, int count)\n";
        glsl << "{\n";
        glsl << "    float distance = 1e6;\n";
        glsl << "    int last = min(first + count, uInstancePositionCount);\n";
        glsl << "    for (int i = first; i < last; ++i) {\n";
        glsl << "        distance = min(distance, length(p - uInstancePositions[i].xyz) - radius);\n";
        glsl << "    }\n";
        glsl << "    return distance;\n";
        glsl << "}\n\n";
    }

    if (result.usesBox) {
        glsl << "float sdf3d_box(vec3 p, vec3 b)\n";
        glsl << "{\n";
        glsl << "    vec3 q = abs(p) - b;\n";
        glsl << "    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);\n";
        glsl << "}\n\n";
    }

    if (result.usesCylinder) {
        glsl << "float sdf3d_cylinder(vec3 p, float radius, float halfHeight)\n";
        glsl << "{\n";
        glsl << "    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(radius, halfHeight);\n";
        glsl << "    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));\n";
        glsl << "}\n\n";
    }

    if (result.usesCappedCone) {
        glsl << "float sdf3d_capped_cone(vec3 p, float radius, float halfHeight)\n";
        glsl << "{\n";
        glsl << "    vec2 q = vec2(length(p.xz), p.y);\n";
        glsl << "    vec2 k1 = vec2(0.0, halfHeight);\n";
        glsl << "    vec2 k2 = vec2(-radius, 2.0 * halfHeight);\n";
        glsl << "    vec2 ca = vec2(q.x - min(q.x, q.y < 0.0 ? radius : 0.0), abs(q.y) - halfHeight);\n";
        glsl << "    vec2 cb = q - k1 + k2 * clamp(dot(k1 - q, k2) / dot(k2, k2), 0.0, 1.0);\n";
        glsl << "    float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;\n";
        glsl << "    return s * sqrt(min(dot(ca, ca), dot(cb, cb)));\n";
        glsl << "}\n\n";
    }

    if (result.usesSmoothMin) {
        glsl << "float sdf3d_smin(float a, float b, float k)\n";
        glsl << "{\n";
        glsl << "    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);\n";
        glsl << "    return mix(b, a, h) - k * h * (1.0 - h);\n";
        glsl << "}\n\n";
    }

    if (result.usesRotate) {
        glsl << glsl_emitter::glslRotationQuaternionFunction();
    }

    glsl << materialGraphFunctions.str();

    for (const GlslSdfHelper& helper : sdfHelpers.helpers) {
        glsl << helper.glsl << "\n";
    }

    glsl << "vec2 sceneSDFWithId(vec3 p)\n";
    glsl << "{\n";
    glsl << sdfWithIdBody;
    glsl << "}\n\n";

    glsl << "float sceneSDF(vec3 p)\n";
    glsl << "{\n";
    glsl << "    return sceneSDFWithId(p).x;\n";
    glsl << "}\n\n";

    glsl << "float sceneNodeSDF(int nodeId, vec3 p)\n";
    glsl << "{\n";
    glsl << "    switch (nodeId) {\n";
    for (const GlslSdfHelper& helper : sdfHelpers.helpers) {
        glsl << "    case " << helper.nodeId << ": return " << helper.functionName << "(p);\n";
    }
    glsl << "    default: return 1e6;\n";
    glsl << "    }\n";
    glsl << "}\n\n";

    glsl << "int scenePickId(vec3 p)\n";
    glsl << "{\n";
    glsl << "    return int(sceneSDFWithId(p).y);\n";
    glsl << "}\n\n";

    glsl << "bool sceneNodeContains(int nodeId, int visibleNodeId)\n";
    glsl << "{\n";
    glsl << "    switch (visibleNodeId) {\n";
    std::unordered_set<uint64_t> emittedContainsCases;
    emitSceneNodeContainsCase(glsl, root, sdfHelpers, emittedContainsCases);
    glsl << "    default: return nodeId == visibleNodeId;\n";
    glsl << "    }\n";
    glsl << "}\n\n";

    glsl << "SdfMaterialSample sceneMaterial(vec3 p)\n";
    glsl << "{\n";
    // AGENT: Deferred material evaluation runs once after hit detection and reuses SDF helpers for distance decisions.
    glsl << materialBody;
    glsl << "}\n";
    result.glsl = glsl.str();

    return result;
}

} // namespace sdf3d
