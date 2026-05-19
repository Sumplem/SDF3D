#include "sdf3d/renderer/PathTraceAccumulation.h"

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

sdf3d::PathTraceFrameKey defaultKey()
{
    sdf3d::PathTraceFrameKey key;
    key.width = 640;
    key.height = 360;
    key.mode = sdf3d::RenderMode::ProgressivePathTrace;
    key.sceneRevision = 1;
    key.materialRevision = 2;
    key.nodeParamRevision = 3;
    return key;
}

void testFrameKeyMatches(std::vector<TestFailure>& failures)
{
    const std::string testName = "path trace frame key matches";
    const sdf3d::PathTraceFrameKey a = defaultKey();
    sdf3d::PathTraceFrameKey b = defaultKey();

    expect(sdf3d::PathTraceAccumulation::frameKeyMatches(a, b), testName, "Expected identical keys to match.", failures);

    b.camera.position.z += 0.25f;
    expect(!sdf3d::PathTraceAccumulation::frameKeyMatches(a, b), testName, "Expected camera change to reset accumulation.", failures);

    b = a;
    b.width = 800;
    expect(!sdf3d::PathTraceAccumulation::frameKeyMatches(a, b), testName, "Expected viewport change to reset accumulation.", failures);

    b = a;
    b.materialRevision += 1;
    expect(!sdf3d::PathTraceAccumulation::frameKeyMatches(a, b), testName, "Expected material change to reset accumulation.", failures);

    b = a;
    b.mode = sdf3d::RenderMode::DirectPreview;
    expect(!sdf3d::PathTraceAccumulation::frameKeyMatches(a, b), testName, "Expected render mode change to reset accumulation.", failures);
}

void testSampleCounter(std::vector<TestFailure>& failures)
{
    const std::string testName = "path trace sample counter";
    sdf3d::PathTraceAccumulation accumulation;

    expect(accumulation.sampleCount() == 0, testName, "Expected zero initial samples.", failures);
    accumulation.markSampleRendered();
    accumulation.markSampleRendered();
    expect(accumulation.sampleCount() == 2, testName, "Expected rendered samples counted.", failures);
    accumulation.reset();
    expect(accumulation.sampleCount() == 0, testName, "Expected reset clears samples.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testFrameKeyMatches(failures);
    testSampleCounter(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All PathTraceAccumulation tests passed.\n";
    return 0;
}
