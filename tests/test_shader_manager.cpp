#include "sdf3d/renderer/ShaderManager.h"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

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
        "uniform int uPathTraceMaxBounces;\n"
        "uniform uint uPathTraceSampleIndex;\n"
        "uniform vec3 uEnvColor;\n"
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
    expect(contains(injected, "uPathTraceMaxBounces"), testName, "Expected path trace bounce uniform preserved.", failures);
    expect(contains(injected, "uPathTraceSampleIndex"), testName, "Expected path tracing uniform preserved.", failures);
    expect(contains(injected, "uEnvColor"), testName, "Expected path trace environment uniform preserved.", failures);
    expect(contains(injected, "secondaryAlbedoScale"), testName, "Expected procedural material field preserved.", failures);
    expect(contains(injected, "sdCapsule"), testName, "Expected gizmo SDF helper preserved.", failures);
    expect(contains(injected, "sdTorus"), testName, "Expected rotate gizmo SDF helper preserved.", failures);
    expect(contains(injected, "sdBox"), testName, "Expected scale gizmo SDF helper preserved.", failures);
    expect(contains(injected, "float sceneSDF"), testName, "Expected scene block injected.", failures);
}

void testWriteInjectedFragmentSource(std::vector<TestFailure>& failures)
{
    const std::string testName = "write injected fragment source";
    const std::filesystem::path outputDirectory = std::filesystem::path("build") / "shader_manager_export_test";
    const std::filesystem::path templatePath = outputDirectory / "template.frag";
    const std::filesystem::path outputPath = outputDirectory / "compiled.frag";
    std::filesystem::create_directories(outputDirectory);

    {
        std::ofstream shaderTemplate(templatePath);
        shaderTemplate
            << "prefix\n"
            << "// SDF3D_SCENE_BEGIN\n"
            << "old\n"
            << "// SDF3D_SCENE_END\n"
            << "suffix\n";
    }

    std::string error;
    const bool written = sdf3d::ShaderManager::writeInjectedFragmentSource(templatePath, "float sceneSDF(vec3 p) { return 1e6; }\n", outputPath, error);
    expect(written, testName, error.empty() ? "Expected export write success." : error, failures);
    expect(error.empty(), testName, "Expected no export error.", failures);

    std::ifstream output(outputPath);
    std::ostringstream contents;
    contents << output.rdbuf();
    const std::string exported = contents.str();
    expect(contains(exported, "float sceneSDF(vec3 p)"), testName, "Expected injected scene GLSL in exported file.", failures);
    expect(contains(exported, "prefix"), testName, "Expected template prefix preserved.", failures);
    expect(contains(exported, "suffix"), testName, "Expected template suffix preserved.", failures);
    expect(!contains(exported, "old"), testName, "Expected old scene block removed.", failures);
}

void testPathTraceShaderCompiles(std::vector<TestFailure>& failures)
{
    const std::string testName = "path trace shader compiles";

    if (glfwInit() != GLFW_TRUE) {
        failures.push_back({testName, "Expected GLFW init for shader compile smoke."});
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(32, 32, "sdf3d shader test", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        failures.push_back({testName, "Expected hidden OpenGL window for shader compile smoke."});
        return;
    }

    glfwMakeContextCurrent(window);
    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
        glfwDestroyWindow(window);
        glfwTerminate();
        failures.push_back({testName, "Expected GLAD to load OpenGL functions."});
        return;
    }

    sdf3d::ShaderManager shaderManager;
    const bool initialized = shaderManager.init(std::filesystem::path("assets") / "shaders");
    const unsigned int pathTraceProgram = initialized ? shaderManager.pathTraceProgram() : 0;
    expect(initialized, testName, shaderManager.lastError().empty() ? "Expected shader manager init." : shaderManager.lastError(), failures);
    expect(pathTraceProgram != 0, testName, shaderManager.lastError().empty() ? "Expected path trace shader program." : shaderManager.lastError(), failures);
    shaderManager.shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testInjectSceneSource(failures);
    testInjectMissingMarkers(failures);
    testEditShaderHasGizmoInjectionShape(failures);
    testWriteInjectedFragmentSource(failures);
    testPathTraceShaderCompiles(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All ShaderManager tests passed.\n";
    return 0;
}
