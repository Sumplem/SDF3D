#include "sdf3d/core/EventBus.h"
#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/systems/GraphSystem.h"

#include <algorithm>
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

bool hasLink(
    const sdf3d::SdfGraph& graph,
    sdf3d::SdfGraphNodeId fromNode,
    const std::string& fromSocket,
    sdf3d::SdfGraphNodeId toNode,
    const std::string& toSocket)
{
    for (const sdf3d::SdfGraphLink& link : graph.links()) {
        if (link.fromNode == fromNode
            && link.fromSocket == fromSocket
            && link.toNode == toNode
            && link.toSocket == toSocket) {
            return true;
        }
    }

    return false;
}

void testDuplicateSelectionCopiesInternalLinksOnly(std::vector<TestFailure>& failures)
{
    const std::string testName = "duplicate selection copies internal links only";
    sdf3d::SdfGraph graph;
    sdf3d::EventBus eventBus;
    int selectionEvents = 0;
    int sceneDirtyEvents = 0;
    std::vector<std::uint64_t> selectedEventIds;

    eventBus.subscribe<sdf3d::SelectionEvent>([&](const sdf3d::SelectionEvent& event) {
        ++selectionEvents;
        selectedEventIds = event.nodeIds;
    });
    eventBus.subscribe<sdf3d::SceneDirtyEvent>([&](const sdf3d::SceneDirtyEvent&) {
        ++sceneDirtyEvents;
    });

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");
    const sdf3d::SdfGraphNodeId box = graph.createNode(sdf3d::SdfNodeType::Box, "External Box");
    const sdf3d::SdfGraphNodeId output = graph.outputNode();

    expect(graph.link(sphere, "sdf", material, "sdf"), testName, "Expected sphere to material link.", failures);
    expect(graph.link(material, "sdf", unionNode, "left"), testName, "Expected material to union link.", failures);
    expect(graph.link(box, "sdf", unionNode, "right"), testName, "Expected external box to union link.", failures);
    expect(graph.link(unionNode, "sdf", output, "surface"), testName, "Expected union to output link.", failures);

    const std::vector<sdf3d::SdfGraphNodeId> duplicates = sdf3d::GraphSystem::duplicateSelection(graph, {sphere, material, unionNode}, eventBus);

    expect(duplicates.size() == 3, testName, "Expected three duplicated nodes.", failures);
    if (duplicates.size() != 3) {
        return;
    }

    const sdf3d::SdfGraphNodeId sphereCopy = duplicates[0];
    const sdf3d::SdfGraphNodeId materialCopy = duplicates[1];
    const sdf3d::SdfGraphNodeId unionCopy = duplicates[2];

    expect(graph.node(sphereCopy) != nullptr, testName, "Expected sphere copy.", failures);
    expect(graph.node(materialCopy) != nullptr, testName, "Expected material copy.", failures);
    expect(graph.node(unionCopy) != nullptr, testName, "Expected union copy.", failures);
    expect(hasLink(graph, sphereCopy, "sdf", materialCopy, "sdf"), testName, "Expected copied sphere to material link.", failures);
    expect(hasLink(graph, materialCopy, "sdf", unionCopy, "left"), testName, "Expected copied material to union link.", failures);
    expect(!hasLink(graph, box, "sdf", unionCopy, "right"), testName, "Expected external incoming link to be dropped.", failures);
    expect(!hasLink(graph, unionCopy, "sdf", output, "surface"), testName, "Expected external outgoing link to be dropped.", failures);
    expect(graph.selectedNodes() == duplicates, testName, "Expected duplicate nodes to become selection.", failures);
    expect(graph.selectedNode() == unionCopy, testName, "Expected last duplicate to become primary selection.", failures);
    expect(selectionEvents == 1, testName, "Expected one selection event.", failures);
    expect(sceneDirtyEvents == 1, testName, "Expected one scene dirty event.", failures);
    expect(selectedEventIds == std::vector<std::uint64_t>(duplicates.begin(), duplicates.end()), testName, "Expected selection event IDs to match duplicates.", failures);
}

void testDuplicateSelectionSkipsOutput(std::vector<TestFailure>& failures)
{
    const std::string testName = "duplicate selection skips output";
    sdf3d::SdfGraph graph;
    sdf3d::EventBus eventBus;
    int sceneDirtyEvents = 0;
    eventBus.subscribe<sdf3d::SceneDirtyEvent>([&](const sdf3d::SceneDirtyEvent&) {
        ++sceneDirtyEvents;
    });

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const std::vector<sdf3d::SdfGraphNodeId> duplicates = sdf3d::GraphSystem::duplicateSelection(graph, {graph.outputNode(), sphere}, eventBus);

    expect(duplicates.size() == 1, testName, "Expected only non-output node duplicated.", failures);
    expect(duplicates.empty() || !graph.isOutputNode(duplicates[0]), testName, "Expected duplicate not to be output node.", failures);
    expect(sceneDirtyEvents == 1, testName, "Expected dirty event for successful duplicate.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testDuplicateSelectionCopiesInternalLinksOnly(failures);
    testDuplicateSelectionSkipsOutput(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All GraphSystem tests passed.\n";
    return 0;
}
