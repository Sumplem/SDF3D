#include "sdf3d/systems/GlslEmitter.h"

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

void testSphereExpression(std::vector<TestFailure>& failures)
{
    const std::string testName = "sphere expression";
    sdf3d::GlslEmitter emitter;
    sdf3d::SdfCompileResult result;
    const std::string expression = emitter.emitNode(sdf3d::makeSphereNode(), "p", result);

    expect(result.errors.empty(), testName, "Expected no emitter errors.", failures);
    expect(result.materials.size() == 1, testName, "Expected default material emission.", failures);
    expect(contains(expression, "length(p) - 1.000000"), testName, "Expected sphere distance expression.", failures);
    expect(contains(expression, ", 0.000000)"), testName, "Expected primitive default material ID.", failures);
}

void testMaterialOverrideExpression(std::vector<TestFailure>& failures)
{
    const std::string testName = "material override expression";
    sdf3d::GlslEmitter emitter;
    sdf3d::SdfCompileResult result;
    sdf3d::SdfNodePtr material = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Red");
    material->material.albedo = {1.0f, 0.0f, 0.0f};
    material->children.push_back(sdf3d::makeSphereNode());
    const std::string expression = emitter.emitNode(material, "p", result);

    expect(result.errors.empty(), testName, "Expected no emitter errors.", failures);
    expect(result.materials.size() == 2, testName, "Expected default plus override material.", failures);
    expect(contains(expression, ", 1.000000)"), testName, "Expected override material ID.", failures);
}

void testNullNode(std::vector<TestFailure>& failures)
{
    const std::string testName = "null node";
    sdf3d::GlslEmitter emitter;
    sdf3d::SdfCompileResult result;
    const std::string expression = emitter.emitNode(nullptr, "p", result);

    expect(!result.errors.empty(), testName, "Expected null node error.", failures);
    expect(expression == "vec2(1e6, 0.0)", testName, "Expected no-hit expression.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testSphereExpression(failures);
    testMaterialOverrideExpression(failures);
    testNullNode(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All GlslEmitter tests passed.\n";
    return 0;
}
