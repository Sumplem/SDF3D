#include "sdf3d/systems/MaterialSystem.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

struct TestFailure {
    std::string name;
    std::string message;
};

void expect(bool condition, const std::string& testName, const std::string& message, std::vector<TestFailure>& failures)
{
    if (!condition) {
        failures.push_back({testName, message});
    }
}

void testAppendOrder(std::vector<TestFailure>& failures)
{
    const std::string testName = "append order";
    sdf3d::MaterialSystem materials;
    sdf3d::SdfCompileResult result;
    sdf3d::SdfMaterial red;
    red.albedo = {1.0f, 0.0f, 0.0f};
    sdf3d::SdfMaterial blue;
    blue.albedo = {0.0f, 0.0f, 1.0f};

    const int defaultId = materials.ensureDefaultMaterial(result);
    const int redId = materials.appendMaterial(result, red);
    const int blueId = materials.appendMaterial(result, blue);

    expect(defaultId == 0, testName, "Expected default material ID 0.", failures);
    expect(redId == 1, testName, "Expected first override material ID 1.", failures);
    expect(blueId == 2, testName, "Expected second override material ID 2.", failures);
    expect(result.materials.size() == 3, testName, "Expected default plus two packed materials.", failures);
    if (result.materials.size() == 3) {
        expect(result.materials[1].material.albedo.x == 1.0f, testName, "Expected first material preserved.", failures);
        expect(result.materials[2].material.albedo.z == 1.0f, testName, "Expected second material preserved.", failures);
    }
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testAppendOrder(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All MaterialSystem tests passed.\n";
    return 0;
}
