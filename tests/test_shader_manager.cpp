#include "sdf3d/renderer/ShaderManager.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

struct TestFailure {
    std::string name;
    std::string message;
};

bool contains(const std::string& text, const std::string& expected)
{
    return text.find(expected) != std::string::npos;
}

void expect(bool condition, const std::string& testName, const std::string& message, std::vector<TestFailure>& failures)
{
    if (!condition) {
        failures.push_back({testName, message});
    }
}

void testInjectSceneSource(std::vector<TestFailure>& failures)
{
    const std::string testName = "inject scene source";
    const std::string shader =
        "void a() {}\n"
        "// SDF3D_SCENE_BEGIN\n"
        "old\n"
        "// SDF3D_SCENE_END\n";
    std::string error;
    const std::string injected = sdf3d::ShaderManager::injectSceneSource(shader, "new\n", error);

    expect(error.empty(), testName, "Expected no injection error.", failures);
    expect(contains(injected, "new\n// SDF3D_SCENE_END"), testName, "Expected injected scene block.", failures);
    expect(!contains(injected, "old"), testName, "Expected old scene block removed.", failures);
}

void testInjectMissingMarkers(std::vector<TestFailure>& failures)
{
    const std::string testName = "inject missing markers";
    std::string error;
    const std::string injected = sdf3d::ShaderManager::injectSceneSource("void main() {}", "new\n", error);

    expect(injected.empty(), testName, "Expected failed injection.", failures);
    expect(!error.empty(), testName, "Expected marker error.", failures);
}

void testEditShaderHasGizmoInjectionShape(std::vector<TestFailure>& failures)
{
    const std::string testName = "edit shader gizmo injection shape";
    const std::string shader =
        "uniform bool uGizmoVisible;\n"
        "uniform int uGizmoRotateStyle;\n"
        "uniform int uRenderQuality;\n"
        "uniform uint uPathTraceSampleIndex;\n"
        "vec4 secondaryAlbedoScale;\n"
        "float sdCapsule(vec3 p, vec3 a, vec3 b, float radius) { return 0.0; }\n"
        "float sdTorus(vec3 p, vec2 t) { return 0.0; }\n"
        "float sdBox(vec3 p, vec3 halfSize) { return 0.0; }\n"
        "// SDF3D_SCENE_BEGIN\n"
        "old\n"
        "// SDF3D_SCENE_END\n";
    std::string error;
    const std::string injected = sdf3d::ShaderManager::injectSceneSource(shader, "float sceneSDF(vec3 p) { return 1e6; }\n", error);

    expect(error.empty(), testName, "Expected no injection error.", failures);
    expect(contains(injected, "uGizmoVisible"), testName, "Expected gizmo uniforms preserved.", failures);
    expect(contains(injected, "uGizmoRotateStyle"), testName, "Expected rotate style uniform preserved.", failures);
    expect(contains(injected, "uRenderQuality"), testName, "Expected quality uniform preserved.", failures);
    expect(contains(injected, "uPathTraceSampleIndex"), testName, "Expected path tracing uniform preserved.", failures);
    expect(contains(injected, "secondaryAlbedoScale"), testName, "Expected procedural material field preserved.", failures);
    expect(contains(injected, "sdCapsule"), testName, "Expected gizmo SDF helper preserved.", failures);
    expect(contains(injected, "sdTorus"), testName, "Expected rotate gizmo SDF helper preserved.", failures);
    expect(contains(injected, "sdBox"), testName, "Expected scale gizmo SDF helper preserved.", failures);
    expect(contains(injected, "float sceneSDF"), testName, "Expected scene block injected.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testInjectSceneSource(failures);
    testInjectMissingMarkers(failures);
    testEditShaderHasGizmoInjectionShape(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All ShaderManager tests passed.\n";
    return 0;
}
