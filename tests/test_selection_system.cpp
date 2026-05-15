#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/systems/SelectionSystem.h"

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

void testGraphSelection(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph selection";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");

    expect(sdf3d::SelectionSystem::setSelectedNode(graph, sphere), testName, "Expected valid graph selection.", failures);
    expect(sdf3d::SelectionSystem::selectedNode(graph) == sphere, testName, "Expected selected graph node.", failures);
    expect(!sdf3d::SelectionSystem::setSelectedNode(graph, 9999), testName, "Expected invalid graph selection to fail.", failures);
}

void testTreeSelection(std::vector<TestFailure>& failures)
{
    const std::string testName = "tree selection";
    sdf3d::SceneGraph sceneGraph;
    sdf3d::SdfNodePtr node = sdf3d::makeSphereNode();

    sdf3d::SelectionSystem::setSelectedNode(sceneGraph, node);

    expect(sdf3d::SelectionSystem::selectedNode(sceneGraph) == node, testName, "Expected selected tree node.", failures);
}

void testDirtyFlag(std::vector<TestFailure>& failures)
{
    const std::string testName = "dirty flag";
    sdf3d::SelectionSystem selection;

    expect(!selection.consumeDirty(), testName, "Expected initial clean state.", failures);
    selection.markDirty();
    expect(selection.consumeDirty(), testName, "Expected dirty state after mark.", failures);
    expect(!selection.consumeDirty(), testName, "Expected dirty state to consume once.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testGraphSelection(failures);
    testTreeSelection(failures);
    testDirtyFlag(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All SelectionSystem tests passed.\n";
    return 0;
}
