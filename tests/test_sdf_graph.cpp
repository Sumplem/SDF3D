#include "sdf3d/scene/SdfGraph.h"

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

void testGraphCreateSelectOutput(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph create select output";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId output = graph.outputNode();
    expect(output != 0, testName, "Expected default output node.", failures);
    expect(graph.selectedNode() == output, testName, "Expected default output selection.", failures);
    expect(graph.isOutputNode(output), testName, "Expected default output marker.", failures);

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");

    expect(sphere != 0, testName, "Expected nonzero graph node ID.", failures);
    expect(graph.outputNode() == output, testName, "Expected default output to remain output.", failures);
    expect(graph.selectedNode() == sphere, testName, "Expected created node to become selected.", failures);
    expect(graph.selectedNodes().size() == 1 && graph.isNodeSelected(sphere), testName, "Expected selected set to contain created node.", failures);
    expect(graph.node(sphere) != nullptr, testName, "Expected node lookup to succeed.", failures);
    if (const sdf3d::SdfGraphNode* node = graph.node(sphere)) {
        expect(node->payload.type == sdf3d::SdfNodeType::Sphere, testName, "Expected node payload type.", failures);
        expect(node->payload.name == "Sphere", testName, "Expected node payload name.", failures);
    }
}

void testGraphLinksAndDelete(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph links and delete";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");

    expect(graph.link(sphere, translate, "child"), testName, "Expected valid link to succeed.", failures);
    expect(graph.links().size() == 1, testName, "Expected one link.", failures);
    expect(!graph.link(sphere, sphere, "child"), testName, "Expected self link to fail.", failures);
    expect(!graph.link(sphere, translate, ""), testName, "Expected empty socket link to fail.", failures);

    const sdf3d::SdfGraphNodeId box = graph.createNode(sdf3d::SdfNodeType::Box, "Box");
    expect(graph.link(box, translate, "child"), testName, "Expected replacement link to succeed.", failures);
    expect(graph.links().size() == 1, testName, "Expected socket link replacement.", failures);
    expect(graph.links().front().fromNode == box, testName, "Expected replacement source node.", failures);

    expect(graph.deleteNode(box), testName, "Expected delete to succeed.", failures);
    expect(graph.links().empty(), testName, "Expected connected links to be removed.", failures);
    expect(graph.node(box) == nullptr, testName, "Expected deleted node lookup to fail.", failures);
    expect(!graph.deleteNode(graph.outputNode()), testName, "Expected output node delete to fail.", failures);
}

void testGraphDuplicateNode(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph duplicate node";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Mat");
    if (sdf3d::SdfGraphNode* node = graph.node(material)) {
        node->payload.material.emission = 3.0f;
        node->editorX = 100.0f;
        node->editorY = 200.0f;
        node->editorPropertiesCollapsed = true;
    }
    graph.link(material, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfGraphNodeId duplicate = graph.duplicateNode(material);

    expect(duplicate != 0, testName, "Expected duplicate node ID.", failures);
    expect(graph.selectedNode() == duplicate, testName, "Expected duplicate to become selected.", failures);
    expect(graph.links().size() == 1, testName, "Expected duplicate not to copy links.", failures);
    const sdf3d::SdfGraphNode* copy = graph.node(duplicate);
    expect(copy != nullptr, testName, "Expected duplicate lookup.", failures);
    if (copy != nullptr) {
        expect(copy->payload.type == sdf3d::SdfNodeType::MaterialOverride, testName, "Expected duplicate type.", failures);
        expect(copy->payload.material.emission == 3.0f, testName, "Expected duplicate material payload.", failures);
        expect(copy->editorX == 132.0f && copy->editorY == 232.0f, testName, "Expected duplicate editor offset.", failures);
        expect(copy->editorPropertiesCollapsed, testName, "Expected duplicate editor collapsed state.", failures);
    }
    expect(graph.duplicateNode(graph.outputNode()) == 0, testName, "Expected output node duplicate to fail.", failures);
}

void testGraphMultiSelectionDeleteCleanup(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph multi selection delete cleanup";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId box = graph.createNode(sdf3d::SdfNodeType::Box, "Box");

    expect(graph.setSelectedNodes({sphere, box}, box), testName, "Expected multi-selection.", failures);
    expect(graph.deleteNode(box), testName, "Expected selected node delete.", failures);
    expect(graph.selectedNode() == sphere, testName, "Expected remaining selected node to become primary.", failures);
    expect(graph.selectedNodes().size() == 1 && graph.isNodeSelected(sphere), testName, "Expected deleted node removed from selected set.", failures);
}

void testGraphExactUnlink(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph exact unlink";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId output = graph.outputNode();

    expect(graph.link(sphere, "sdf", output, "surface"), testName, "Expected output link to succeed.", failures);
    expect(graph.links().size() == 1, testName, "Expected one output link.", failures);
    expect(!graph.unlink(sphere, "missing", output, "surface"), testName, "Expected missing exact link unlink to fail.", failures);
    expect(graph.unlink(sphere, "sdf", output, "surface"), testName, "Expected exact unlink to succeed.", failures);
    expect(graph.links().empty(), testName, "Expected exact unlink to remove link.", failures);
}

void testGraphSocketsAndTypedLinks(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph sockets and typed links";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");

    const sdf3d::SdfGraphNode* sphereNode = graph.node(sphere);
    const sdf3d::SdfGraphNode* translateNode = graph.node(translate);
    expect(sphereNode != nullptr, testName, "Expected sphere node.", failures);
    expect(translateNode != nullptr, testName, "Expected translate node.", failures);

    if (sphereNode != nullptr && translateNode != nullptr) {
        expect(sphereNode->inputs.empty(), testName, "Expected primitive to have no SDF inputs.", failures);
        expect(sphereNode->outputs.size() == 1, testName, "Expected primitive SDF output.", failures);
        expect(sphereNode->outputs.front().name == "sdf", testName, "Expected primitive output socket name.", failures);
        expect(translateNode->inputs.size() == 1, testName, "Expected transform child input.", failures);
        expect(translateNode->inputs.front().name == "child", testName, "Expected transform input socket name.", failures);
    }

    expect(graph.link(sphere, "sdf", translate, "child"), testName, "Expected typed SDF link to succeed.", failures);
    expect(!graph.link(sphere, "missing", translate, "child"), testName, "Expected missing output socket to fail.", failures);
    expect(!graph.link(sphere, "sdf", translate, "missing"), testName, "Expected missing input socket to fail.", failures);
}

void testGraphOutputNodeSockets(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph output node sockets";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId output = graph.outputNode();
    const sdf3d::SdfGraphNode* outputNode = graph.node(output);

    expect(outputNode != nullptr, testName, "Expected output node.", failures);
    if (outputNode != nullptr) {
        expect(outputNode->inputs.size() == 1, testName, "Expected one output input socket.", failures);
        expect(outputNode->inputs.front().name == "surface", testName, "Expected surface input socket.", failures);
        expect(outputNode->outputs.empty(), testName, "Expected no output sockets.", failures);
    }
    expect(graph.outputNode() == output, testName, "Expected output node to become graph output.", failures);
}

void testGraphOutputAndSelectionValidation(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph output and selection validation";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");

    expect(!graph.setOutputNode(9999), testName, "Expected invalid output node to fail.", failures);
    expect(!graph.setSelectedNode(9999), testName, "Expected invalid selected node to fail.", failures);
    const sdf3d::SdfGraphNodeId output = graph.outputNode();
    expect(!graph.setOutputNode(0), testName, "Expected clearing output to fail.", failures);
    expect(graph.outputNode() == output, testName, "Expected output to stay default output.", failures);
    expect(graph.setSelectedNode(0), testName, "Expected clearing selection to succeed.", failures);
    expect(graph.selectedNode() == 0, testName, "Expected selection to be cleared.", failures);
    expect(!graph.setOutputNode(sphere), testName, "Expected non-output node output assignment to fail.", failures);
    expect(graph.outputNode() == output, testName, "Expected output to remain output node.", failures);
}

void testPhaseTwoDomainNodeDefinitions(std::vector<TestFailure>& failures)
{
    const std::string testName = "phase two domain node definitions";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId repeat = graph.createNode(sdf3d::SdfNodeType::Repeat, "Repeat");
    const sdf3d::SdfGraphNodeId mirror = graph.createNode(sdf3d::SdfNodeType::Mirror, "Mirror");
    const sdf3d::SdfGraphNodeId twist = graph.createNode(sdf3d::SdfNodeType::Twist, "Twist");
    const sdf3d::SdfGraphNodeId bend = graph.createNode(sdf3d::SdfNodeType::Bend, "Bend");
    const sdf3d::SdfGraphNode* repeatNode = graph.node(repeat);
    const sdf3d::SdfGraphNode* mirrorNode = graph.node(mirror);
    const sdf3d::SdfGraphNode* twistNode = graph.node(twist);
    const sdf3d::SdfGraphNode* bendNode = graph.node(bend);

    expect(repeatNode != nullptr, testName, "Expected repeat node.", failures);
    expect(mirrorNode != nullptr, testName, "Expected mirror node.", failures);
    if (repeatNode != nullptr) {
        expect(repeatNode->inputs.size() == 1 && repeatNode->inputs[0].name == "child", testName, "Expected repeat child input.", failures);
        expect(repeatNode->outputs.size() == 1 && repeatNode->outputs[0].name == "sdf", testName, "Expected repeat SDF output.", failures);
        expect(repeatNode->payload.parameters.at("x") == 2.0f, testName, "Expected repeat default cell x.", failures);
    }
    if (mirrorNode != nullptr) {
        expect(mirrorNode->inputs.size() == 1 && mirrorNode->inputs[0].name == "child", testName, "Expected mirror child input.", failures);
        expect(mirrorNode->outputs.size() == 1 && mirrorNode->outputs[0].name == "sdf", testName, "Expected mirror SDF output.", failures);
        expect(mirrorNode->payload.parameters.at("x") == 1.0f, testName, "Expected mirror x enabled by default.", failures);
    }
    if (twistNode != nullptr) {
        expect(twistNode->inputs.size() == 1 && twistNode->inputs[0].name == "child", testName, "Expected twist child input.", failures);
        expect(twistNode->outputs.size() == 1 && twistNode->outputs[0].name == "sdf", testName, "Expected twist SDF output.", failures);
        expect(twistNode->payload.parameters.at("strength") == 1.0f, testName, "Expected twist default strength.", failures);
    }
    if (bendNode != nullptr) {
        expect(bendNode->inputs.size() == 1 && bendNode->inputs[0].name == "child", testName, "Expected bend child input.", failures);
        expect(bendNode->outputs.size() == 1 && bendNode->outputs[0].name == "sdf", testName, "Expected bend SDF output.", failures);
        expect(bendNode->payload.parameters.at("strength") == 0.5f, testName, "Expected bend default strength.", failures);
    }
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testGraphCreateSelectOutput(failures);
    testGraphLinksAndDelete(failures);
    testGraphDuplicateNode(failures);
    testGraphMultiSelectionDeleteCleanup(failures);
    testGraphExactUnlink(failures);
    testGraphSocketsAndTypedLinks(failures);
    testGraphOutputNodeSockets(failures);
    testGraphOutputAndSelectionValidation(failures);
    testPhaseTwoDomainNodeDefinitions(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All SDF graph tests passed.\n";
    return 0;
}
