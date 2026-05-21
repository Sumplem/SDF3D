#include "sdf3d/renderer/FboRenderer.h"

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

void testNormalizedDimension(std::vector<TestFailure>& failures)
{
    const std::string testName = "normalized dimension";

    expect(sdf3d::FboRenderer::normalizedDimension(128) == 128, testName, "Expected positive dimension preserved.", failures);
    expect(sdf3d::FboRenderer::normalizedDimension(0) == 1, testName, "Expected zero dimension clamped.", failures);
    expect(sdf3d::FboRenderer::normalizedDimension(-5) == 1, testName, "Expected negative dimension clamped.", failures);
}

void testNodeIdAttachmentDefaults(std::vector<TestFailure>& failures)
{
    const std::string testName = "node id attachment defaults";
    const sdf3d::FboRenderer renderer;

    expect(renderer.nodeIdTexture() == 0, testName, "Expected node id texture absent before init.", failures);
    expect(renderer.readNodeIdPixel(0, 0) == -1, testName, "Expected node id read to return no-hit before init.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testNormalizedDimension(failures);
    testNodeIdAttachmentDefaults(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All FboRenderer tests passed.\n";
    return 0;
}
