#include "sdf3d/renderer/UniformUploader.h"

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

void testMaterialCountCap(std::vector<TestFailure>& failures)
{
    const std::string testName = "material count cap";

    expect(sdf3d::UniformUploader::materialCountForShader(0) == 0, testName, "Expected zero materials.", failures);
    expect(sdf3d::UniformUploader::materialCountForShader(63) == 63, testName, "Expected under-cap count preserved.", failures);
    expect(sdf3d::UniformUploader::materialCountForShader(64) == 64, testName, "Expected cap count preserved.", failures);
    expect(sdf3d::UniformUploader::materialCountForShader(65) == 64, testName, "Expected over-cap count clamped.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testMaterialCountCap(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All UniformUploader tests passed.\n";
    return 0;
}
