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

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testInjectSceneSource(failures);
    testInjectMissingMarkers(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All ShaderManager tests passed.\n";
    return 0;
}
