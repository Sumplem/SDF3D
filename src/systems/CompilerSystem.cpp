#include "sdf3d/systems/CompilerSystem.h"

#include "sdf3d/scene/SdfGraphCompiler.h"
#include "sdf3d/systems/GlslEmitter.h"
#include "sdf3d/systems/GraphSystem.h"

#include "glsl_emitter/GlslEmitterMath.h"

#include <sstream>

namespace sdf3d {

SdfCompileResult CompilerSystem::compile(const SdfGraph& graph) const
{
    const SdfGraphLowerResult lowered = lowerSdfGraphToTree(graph);
    SdfCompileResult result = compile(lowered.root);
    result.errors.insert(result.errors.begin(), lowered.errors.begin(), lowered.errors.end());
    result.nodeParams = GraphSystem::collectNodeParams(graph);
    return result;
}

SdfCompileResult CompilerSystem::compile(const SdfNodePtr& root) const
{
    SdfCompileResult result;

    if (!root) {
        result.errors.push_back("Cannot compile an empty SDF tree.");
        // AGENT: Empty scenes still emit both runtime entry points so the shader
        // template stays valid before the user adds geometry.
        result.glsl =
            "float sceneSDF(vec3 p)\n"
            "{\n"
            "    return 1e6;\n"
            "}\n\n"
            "SdfMaterialSample sceneMaterial(vec3 p)\n"
            "{\n"
            "    return sampleMaterial(0, p);\n"
            "}\n\n"
            "float sceneNodeSDF(int nodeId, vec3 p)\n"
            "{\n"
            "    return 1e6;\n"
            "}\n";
        return result;
    }

    const GlslEmitter emitter;
    const GlslSdfHelperBlock sdfHelpers = emitter.emitSdfHelpers(root, result);
    const std::string materialExpression = emitter.emitSceneMaterialExpression(root, "p", result, sdfHelpers);

    std::ostringstream glsl;
    glsl << "struct SdfNodeParam\n";
    glsl << "{\n";
    glsl << "    uvec4 id;\n";
    glsl << "    vec4 data0;\n";
    glsl << "};\n\n";
    glsl << "layout(std430, binding = 1) readonly buffer NodeParamBuffer\n";
    glsl << "{\n";
    glsl << "    SdfNodeParam uNodeParams[];\n";
    glsl << "};\n\n";
    glsl << "uniform int uNodeParamCount;\n\n";
    glsl << "vec4 sdf3d_nodeParam0(uint nodeIdLow, uint nodeIdHigh, vec4 fallback)\n";
    glsl << "{\n";
    glsl << "    for (int i = 0; i < uNodeParamCount; ++i) {\n";
    glsl << "        if (uNodeParams[i].id.x == nodeIdLow && uNodeParams[i].id.y == nodeIdHigh) {\n";
    glsl << "            return uNodeParams[i].data0;\n";
    glsl << "        }\n";
    glsl << "    }\n";
    glsl << "    return fallback;\n";
    glsl << "}\n\n";

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

    for (const GlslSdfHelper& helper : sdfHelpers.helpers) {
        glsl << helper.glsl << "\n";
    }

    glsl << "float sceneSDF(vec3 p)\n";
    glsl << "{\n";
    glsl << "    return " << sdfHelpers.rootFunctionName << "(p);\n";
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

    glsl << "SdfMaterialSample sceneMaterial(vec3 p)\n";
    glsl << "{\n";
    // AGENT: Deferred material evaluation runs once after hit detection and reuses SDF helpers for distance decisions.
    glsl << "    return " << materialExpression << ";\n";
    glsl << "}\n";
    result.glsl = glsl.str();

    return result;
}

} // namespace sdf3d
