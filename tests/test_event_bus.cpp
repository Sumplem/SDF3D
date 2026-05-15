#include "sdf3d/core/EventBus.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

struct TestFailure {
    std::string name;
    std::string message;
};

struct CustomEvent {
    int value = 0;
};

void expect(bool condition, const std::string& testName, const std::string& message, std::vector<TestFailure>& failures)
{
    if (!condition) {
        failures.push_back({testName, message});
    }
}

void testSubscribeEmit(std::vector<TestFailure>& failures)
{
    const std::string testName = "subscribe emit";
    sdf3d::EventBus bus;
    int received = 0;

    bus.subscribe<CustomEvent>([&received](const CustomEvent& event) {
        received = event.value;
    });
    bus.emit(CustomEvent{42});

    expect(received == 42, testName, "Expected subscriber to receive event payload.", failures);
}

void testUnsubscribe(std::vector<TestFailure>& failures)
{
    const std::string testName = "unsubscribe";
    sdf3d::EventBus bus;
    int calls = 0;

    const sdf3d::EventBus::SubscriptionId id = bus.subscribe<sdf3d::SceneDirtyEvent>([&calls](const sdf3d::SceneDirtyEvent&) {
        ++calls;
    });

    expect(bus.unsubscribe<sdf3d::SceneDirtyEvent>(id), testName, "Expected unsubscribe to remove handler.", failures);
    bus.emit(sdf3d::SceneDirtyEvent{});

    expect(calls == 0, testName, "Expected unsubscribed handler not to run.", failures);
}

void testClear(std::vector<TestFailure>& failures)
{
    const std::string testName = "clear";
    sdf3d::EventBus bus;
    int calls = 0;

    bus.subscribe<sdf3d::SceneDirtyEvent>([&calls](const sdf3d::SceneDirtyEvent&) {
        ++calls;
    });
    bus.clear();
    bus.emit(sdf3d::SceneDirtyEvent{});

    expect(calls == 0, testName, "Expected clear to remove all handlers.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testSubscribeEmit(failures);
    testUnsubscribe(failures);
    testClear(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All EventBus tests passed.\n";
    return 0;
}
