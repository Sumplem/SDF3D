#include "sdf3d/core/EventBus.h"
#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/systems/GraphSystem.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

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

void testEffectiveValidityDropsInvalidUpstream(std::vector<TestFailure>& failures)
{
    const std::string testName = "effective validity drops invalid upstream";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId emptyTranslate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");

    expect(graph.link(sphere, "sdf", unionNode, "left"), testName, "Expected valid left link.", failures);
    expect(graph.link(emptyTranslate, "sdf", unionNode, "right"), testName, "Expected invalid right link.", failures);

    expect(sdf3d::GraphSystem::producesValidSdf(graph, sphere), testName, "Expected primitive to produce valid SDF.", failures);
    expect(!sdf3d::GraphSystem::producesValidSdf(graph, emptyTranslate), testName, "Expected transform without child to be invalid.", failures);
    expect(sdf3d::GraphSystem::producesValidSdf(graph, unionNode), testName, "Expected union to bypass invalid input.", failures);
    expect(sdf3d::GraphSystem::effectiveLinkToInput(graph, unionNode, "left").has_value(), testName, "Expected valid effective left input.", failures);
    expect(!sdf3d::GraphSystem::effectiveLinkToInput(graph, unionNode, "right").has_value(), testName, "Expected invalid effective right input to drop.", failures);

    const sdf3d::SdfGraphNode* unionGraphNode = graph.node(unionNode);
    expect(unionGraphNode != nullptr && sdf3d::GraphSystem::nodeHasMissingRequiredInput(graph, *unionGraphNode), testName, "Expected UI missing-input query to flag bypass visual.", failures);
}

void testLoweredRequiredInputRules(std::vector<TestFailure>& failures)
{
    const std::string testName = "lowered required input rules";

    expect(sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::Union, {"left"}, 1), testName, "Expected single-input union valid.", failures);
    expect(sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::Subtract, {"base"}, 1), testName, "Expected subtract with base valid.", failures);
    expect(!sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::Translate, {}, 0), testName, "Expected transform without child invalid.", failures);
    expect(!sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::MaterialOverride, {}, 0), testName, "Expected material override without sdf invalid.", failures);
}

void testPickNodeByRaySelectsTranslatedPrimitive(std::vector<TestFailure>& failures)
{
    const std::string testName = "pick node by ray selects translated primitive";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    if (sdf3d::SdfGraphNode* node = graph.node(translate)) {
        node->payload.parameters["x"] = 1.5f;
    }
    expect(graph.link(sphere, "sdf", translate, "child"), testName, "Expected sphere to translate link.", failures);
    expect(graph.link(translate, "sdf", graph.outputNode(), "surface"), testName, "Expected translate to output link.", failures);

    const sdf3d::SdfGraphNodeId picked = sdf3d::GraphSystem::pickNodeByRay(
        graph,
        {1.5f, 0.0f, 5.0f},
        glm::normalize(glm::vec3{0.0f, 0.0f, -1.0f}));
    expect(picked == sphere, testName, "Expected ray pick to return primitive node through transform.", failures);
}

void testPickNodeByRayMissesEmptySpace(std::vector<TestFailure>& failures)
{
    const std::string testName = "pick node by ray misses empty space";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    expect(graph.link(sphere, "sdf", graph.outputNode(), "surface"), testName, "Expected sphere to output link.", failures);

    const sdf3d::SdfGraphNodeId picked = sdf3d::GraphSystem::pickNodeByRay(
        graph,
        {4.0f, 0.0f, 5.0f},
        glm::normalize(glm::vec3{0.0f, 0.0f, -1.0f}));
    expect(picked == 0, testName, "Expected off-axis ray to miss sphere.", failures);
}

void testHighlightNodeForSelectionUsesTransformWrapper(std::vector<TestFailure>& failures)
{
    const std::string testName = "highlight node for selection uses transform wrapper";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    expect(graph.link(sphere, "sdf", translate, "child"), testName, "Expected sphere to translate link.", failures);
    expect(graph.link(translate, "sdf", graph.outputNode(), "surface"), testName, "Expected translate to output link.", failures);

    graph.setSelectedNode(sphere);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == translate, testName, "Expected primitive selection to highlight transform wrapper.", failures);

    graph.setSelectedNode(translate);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == translate, testName, "Expected transform selection to highlight itself.", failures);
}

void testHighlightNodeForSelectionFollowsTransformChain(std::vector<TestFailure>& failures)
{
    const std::string testName = "highlight node for selection follows transform chain";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    const sdf3d::SdfGraphNodeId rotate = graph.createNode(sdf3d::SdfNodeType::Rotate, "Rotate");
    const sdf3d::SdfGraphNodeId scale = graph.createNode(sdf3d::SdfNodeType::Scale, "Scale");
    expect(graph.link(sphere, "sdf", translate, "child"), testName, "Expected sphere to translate link.", failures);
    expect(graph.link(translate, "sdf", rotate, "child"), testName, "Expected translate to rotate link.", failures);
    expect(graph.link(rotate, "sdf", scale, "child"), testName, "Expected rotate to scale link.", failures);
    expect(graph.link(scale, "sdf", graph.outputNode(), "surface"), testName, "Expected scale to output link.", failures);

    graph.setSelectedNode(sphere);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == scale, testName, "Expected primitive selection to highlight final transform in chain.", failures);

    graph.setSelectedNode(translate);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == scale, testName, "Expected inner transform selection to highlight final visible chain.", failures);

    graph.setSelectedNode(rotate);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == scale, testName, "Expected middle transform selection to highlight final visible chain.", failures);

    graph.setSelectedNode(scale);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == scale, testName, "Expected top transform selection to highlight itself.", failures);
}

void testEnsureTransformWrapperCanChainFromTransform(std::vector<TestFailure>& failures)
{
    const std::string testName = "ensure transform wrapper can chain from transform";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    expect(graph.link(sphere, "sdf", translate, "child"), testName, "Expected sphere to translate link.", failures);
    expect(graph.link(translate, "sdf", graph.outputNode(), "surface"), testName, "Expected translate to output link.", failures);

    const sdf3d::SdfGraphNodeId rotate = sdf3d::GraphSystem::ensureRotateWrapperForNode(graph, translate);

    expect(rotate != 0, testName, "Expected rotate wrapper for selected transform.", failures);
    expect(hasLink(graph, sphere, "sdf", translate, "child"), testName, "Expected primitive to stay linked to translate.", failures);
    expect(hasLink(graph, translate, "sdf", rotate, "child"), testName, "Expected translate to feed rotate.", failures);
    expect(hasLink(graph, rotate, "sdf", graph.outputNode(), "surface"), testName, "Expected rotate to feed output.", failures);
    expect(graph.selectedNode() == rotate, testName, "Expected new rotate wrapper selected.", failures);
}

void testEnsureTransformWrapperReusesExistingChainParent(std::vector<TestFailure>& failures)
{
    const std::string testName = "ensure transform wrapper reuses existing chain parent";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    const sdf3d::SdfGraphNodeId rotate = graph.createNode(sdf3d::SdfNodeType::Rotate, "Rotate");
    expect(graph.link(sphere, "sdf", translate, "child"), testName, "Expected sphere to translate link.", failures);
    expect(graph.link(translate, "sdf", rotate, "child"), testName, "Expected translate to rotate link.", failures);
    expect(graph.link(rotate, "sdf", graph.outputNode(), "surface"), testName, "Expected rotate to output link.", failures);

    const sdf3d::SdfGraphNodeId reused = sdf3d::GraphSystem::ensureRotateWrapperForNode(graph, sphere);

    expect(reused == rotate, testName, "Expected primitive selection to reuse rotate in transform chain.", failures);
    expect(graph.links().size() == 3, testName, "Expected no duplicate wrapper links.", failures);
    expect(graph.selectedNode() == rotate, testName, "Expected existing rotate selected.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testDuplicateSelectionCopiesInternalLinksOnly(failures);
    testDuplicateSelectionSkipsOutput(failures);
    testEffectiveValidityDropsInvalidUpstream(failures);
    testLoweredRequiredInputRules(failures);
    testPickNodeByRaySelectsTranslatedPrimitive(failures);
    testPickNodeByRayMissesEmptySpace(failures);
    testHighlightNodeForSelectionUsesTransformWrapper(failures);
    testHighlightNodeForSelectionFollowsTransformChain(failures);
    testEnsureTransformWrapperCanChainFromTransform(failures);
    testEnsureTransformWrapperReusesExistingChainParent(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All GraphSystem tests passed.\n";
    return 0;
}
