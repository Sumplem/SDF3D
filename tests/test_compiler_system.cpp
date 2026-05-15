#include "sdf3d/systems/CompilerSystem.h"

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

void testCompileSphere(std::vector<TestFailure>& failures)
{
    const std::string testName = "compile sphere";
    const sdf3d::CompilerSystem compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(sdf3d::makeSphereNode());

    expect(result.errors.empty(), testName, "Expected no compile errors.", failures);
    expect(contains(result.glsl, "sceneSDFWithMaterial"), testName, "Expected material entry point.", failures);
    expect(result.materials.size() == 1, testName, "Expected default material output.", failures);
}

void testCompileMaterialOverride(std::vector<TestFailure>& failures)
{
    const std::string testName = "compile material override";
    const sdf3d::CompilerSystem compiler;
    sdf3d::SdfNodePtr material = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Red");
    material->material.albedo = {1.0f, 0.0f, 0.0f};
    material->children.push_back(sdf3d::makeSphereNode());
    const sdf3d::SdfCompileResult result = compiler.compile(material);

    expect(result.errors.empty(), testName, "Expected no compile errors.", failures);
    expect(result.materials.size() == 2, testName, "Expected default plus override material.", failures);
    if (result.materials.size() == 2) {
        expect(result.materials[1].material.albedo.x == 1.0f, testName, "Expected override material preserved.", failures);
    }
}

void testCompileEmpty(std::vector<TestFailure>& failures)
{
    const std::string testName = "compile empty";
    const sdf3d::CompilerSystem compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(nullptr);

    expect(!result.errors.empty(), testName, "Expected empty compile error.", failures);
    expect(contains(result.glsl, "return 1e6;"), testName, "Expected no-hit fallback.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testCompileSphere(failures);
    testCompileMaterialOverride(failures);
    testCompileEmpty(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All CompilerSystem tests passed.\n";
    return 0;
}
