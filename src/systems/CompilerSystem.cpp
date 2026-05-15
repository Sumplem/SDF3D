#include "sdf3d/systems/CompilerSystem.h"

#include "sdf3d/scene/SdfGraphCompiler.h"
#include "sdf3d/systems/GlslEmitter.h"

#include <sstream>

namespace sdf3d {

SdfCompileResult CompilerSystem::compile(const SdfGraph& graph) const
{
    const SdfGraphLowerResult lowered = lowerSdfGraphToTree(graph);
    SdfCompileResult result = compile(lowered.root);
    result.errors.insert(result.errors.begin(), lowered.errors.begin(), lowered.errors.end());
    return result;
}

SdfCompileResult CompilerSystem::compile(const SdfNodePtr& root) const
{
    SdfCompileResult result;

    if (!root) {
        result.errors.push_back("Cannot compile an empty SDF tree.");
        // AGENT: Empty scenes still emit the material-aware entry point so the
        // runtime shader template stays valid before the user adds geometry.
        result.glsl =
            "vec2 sceneSDFWithMaterial(vec3 p)\n"
            "{\n"
            "    return vec2(1e6, 0.0);\n"
            "}\n\n"
            "float sceneSDF(vec3 p)\n"
            "{\n"
            "    return 1e6;\n"
            "}\n";
        return result;
    }

    const GlslEmitter emitter;
    const std::string expression = emitter.emitNode(root, "p", result);

    std::ostringstream glsl;
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
        glsl << "mat3 sdf3d_rotationXYZ(vec3 degrees)\n";
        glsl << "{\n";
        glsl << "    vec3 r = radians(degrees);\n";
        glsl << "    vec3 c = cos(r);\n";
        glsl << "    vec3 s = sin(r);\n";
        glsl << "    mat3 rx = mat3(1.0, 0.0, 0.0, 0.0, c.x, s.x, 0.0, -s.x, c.x);\n";
        glsl << "    mat3 ry = mat3(c.y, 0.0, -s.y, 0.0, 1.0, 0.0, s.y, 0.0, c.y);\n";
        glsl << "    mat3 rz = mat3(c.z, s.z, 0.0, -s.z, c.z, 0.0, 0.0, 0.0, 1.0);\n";
        glsl << "    return rz * ry * rx;\n";
        glsl << "}\n\n";
    }

    glsl << "vec2 sceneSDFWithMaterial(vec3 p)\n";
    glsl << "{\n";
    glsl << "    return " << expression << ";\n";
    glsl << "}\n\n";

    glsl << "float sceneSDF(vec3 p)\n";
    glsl << "{\n";
    glsl << "    return sceneSDFWithMaterial(p).x;\n";
    glsl << "}\n";
    result.glsl = glsl.str();

    return result;
}

} // namespace sdf3d
