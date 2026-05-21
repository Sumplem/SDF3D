#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/systems/GraphSystem.h"
#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"
#include "sdf3d/ui/node_editor/NodeEditorLayout.h"
#include "sdf3d/ui/node_editor/NodeEditorProperties.h"

#include <cmath>
#include <iostream>
#include <glm/glm.hpp>
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

void expectVec3(glm::vec3 actual, glm::vec3 expected, const std::string& testName, const std::string& message, std::vector<TestFailure>& failures)
{
    constexpr float epsilon = 0.0001f;
    expect(
        std::abs(actual.x - expected.x) <= epsilon
            && std::abs(actual.y - expected.y) <= epsilon
            && std::abs(actual.z - expected.z) <= epsilon,
        testName,
        message,
        failures);
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

void testGraphSystemCreatesTranslateWrapper(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph system creates translate wrapper";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    if (sdf3d::SdfGraphNode* node = graph.node(sphere)) {
        node->editorX = 100.0f;
        node->editorY = 200.0f;
    }
    graph.link(sphere, "sdf", material, "sdf");
    graph.link(material, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfGraphNodeId translate = sdf3d::GraphSystem::ensureTranslateWrapperForNode(graph, sphere);

    expect(translate != 0, testName, "Expected Translate wrapper.", failures);
    expect(graph.selectedNode() == translate, testName, "Expected Translate wrapper selected.", failures);
    const sdf3d::SdfGraphNode* translateNode = graph.node(translate);
    expect(translateNode != nullptr && translateNode->payload.type == sdf3d::SdfNodeType::Translate, testName, "Expected Translate node type.", failures);
    expect(translateNode != nullptr && translateNode->editorX == 360.0f && translateNode->editorY == 200.0f, testName, "Expected wrapper editor position.", failures);

    bool hasChildLink = false;
    bool hasOutgoingLink = false;
    bool oldOutgoingRemoved = true;
    for (const sdf3d::SdfGraphLink& link : graph.links()) {
        hasChildLink = hasChildLink || (link.fromNode == sphere && link.fromSocket == "sdf" && link.toNode == translate && link.toSocket == "child");
        hasOutgoingLink = hasOutgoingLink || (link.fromNode == translate && link.fromSocket == "sdf" && link.toNode == material && link.toSocket == "sdf");
        if (link.fromNode == sphere && link.toNode == material) {
            oldOutgoingRemoved = false;
        }
    }
    expect(hasChildLink, testName, "Expected primitive linked into wrapper child.", failures);
    expect(hasOutgoingLink, testName, "Expected wrapper to feed old consumer.", failures);
    expect(oldOutgoingRemoved, testName, "Expected old primitive consumer link removed.", failures);
}

void testGraphSystemReusesTranslateWrapper(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph system reuses translate wrapper";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    graph.link(sphere, "sdf", translate, "child");

    const sdf3d::SdfGraphNodeId reused = sdf3d::GraphSystem::ensureTranslateWrapperForNode(graph, sphere);

    expect(reused == translate, testName, "Expected existing Translate wrapper reused.", failures);
    expect(graph.selectedNode() == translate, testName, "Expected existing Translate selected.", failures);
    expect(graph.nodes().size() == 3, testName, "Expected no extra node beyond Output, Sphere, Translate.", failures);
}

void testGraphSystemCreatesRotateWrapper(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph system creates rotate wrapper";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    graph.link(sphere, "sdf", material, "sdf");
    graph.link(material, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfGraphNodeId rotate = sdf3d::GraphSystem::ensureRotateWrapperForNode(graph, sphere);

    expect(rotate != 0, testName, "Expected Rotate wrapper.", failures);
    expect(graph.selectedNode() == rotate, testName, "Expected Rotate wrapper selected.", failures);
    const sdf3d::SdfGraphNode* rotateNode = graph.node(rotate);
    expect(rotateNode != nullptr && rotateNode->payload.type == sdf3d::SdfNodeType::Rotate, testName, "Expected Rotate node type.", failures);

    bool hasChildLink = false;
    bool hasOutgoingLink = false;
    bool oldOutgoingRemoved = true;
    for (const sdf3d::SdfGraphLink& link : graph.links()) {
        hasChildLink = hasChildLink || (link.fromNode == sphere && link.fromSocket == "sdf" && link.toNode == rotate && link.toSocket == "child");
        hasOutgoingLink = hasOutgoingLink || (link.fromNode == rotate && link.fromSocket == "sdf" && link.toNode == material && link.toSocket == "sdf");
        if (link.fromNode == sphere && link.toNode == material) {
            oldOutgoingRemoved = false;
        }
    }
    expect(hasChildLink, testName, "Expected primitive linked into Rotate child.", failures);
    expect(hasOutgoingLink, testName, "Expected Rotate to feed old consumer.", failures);
    expect(oldOutgoingRemoved, testName, "Expected old primitive consumer link removed.", failures);
}

void testGraphSystemReusesRotateWrapper(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph system reuses rotate wrapper";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId rotate = graph.createNode(sdf3d::SdfNodeType::Rotate, "Rotate");
    graph.link(sphere, "sdf", rotate, "child");

    const sdf3d::SdfGraphNodeId reused = sdf3d::GraphSystem::ensureRotateWrapperForNode(graph, sphere);

    expect(reused == rotate, testName, "Expected existing Rotate wrapper reused.", failures);
    expect(graph.selectedNode() == rotate, testName, "Expected existing Rotate selected.", failures);
    expect(graph.nodes().size() == 3, testName, "Expected no extra node beyond Output, Sphere, Rotate.", failures);
}

void testRotateQuaternionParamsAreHiddenMetadata(std::vector<TestFailure>& failures)
{
    const std::string testName = "rotate quaternion params are hidden metadata";
    const sdf3d::SdfNodePtr rotate = sdf3d::makeSdfNodeFromDefinition(sdf3d::SdfNodeType::Rotate);

    expect(rotate != nullptr, testName, "Expected rotate node.", failures);
    if (rotate == nullptr) {
        return;
    }

    expect(rotate->parameters.find(sdf3d::RotateParamQx) != rotate->parameters.end(), testName, "Expected qx runtime param.", failures);
    expect(rotate->parameters.find(sdf3d::RotateParamQw) != rotate->parameters.end(), testName, "Expected qw runtime param.", failures);
    expect(!sdf3d::isSdfParameterVisible(rotate->type, sdf3d::RotateParamQx), testName, "Expected qx hidden by metadata.", failures);
    expect(!sdf3d::isSdfParameterVisible(rotate->type, sdf3d::RotateParamQw), testName, "Expected qw hidden by metadata.", failures);
    expect(sdf3d::node_editor::visibleInlinePropertyParameterCount(*rotate) == 3, testName, "Expected only Euler params visible.", failures);
}

void testMaterialOverrideGetsRegistryMaterial(std::vector<TestFailure>& failures)
{
    const std::string testName = "material node gets registry material";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::SolidMaterial, "Paint");
    const sdf3d::SdfGraphNode* node = graph.node(material);

    expect(node != nullptr, testName, "Expected material node.", failures);
    expect(node != nullptr && node->payload.materialId != 0, testName, "Expected material id assigned.", failures);
    if (node != nullptr) {
        const sdf3d::MaterialDefinition* definition = graph.materials().material(node->payload.materialId);
        expect(definition != nullptr, testName, "Expected registry material.", failures);
        if (definition != nullptr) {
            expect(definition->name == "Paint", testName, "Expected registry material name.", failures);
        }
    }
}

void testGraphSystemCreatesScaleWrapper(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph system creates scale wrapper";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    graph.link(sphere, "sdf", material, "sdf");
    graph.link(material, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfGraphNodeId scale = sdf3d::GraphSystem::ensureScaleWrapperForNode(graph, sphere);

    expect(scale != 0, testName, "Expected Scale wrapper.", failures);
    expect(graph.selectedNode() == scale, testName, "Expected Scale wrapper selected.", failures);
    const sdf3d::SdfGraphNode* scaleNode = graph.node(scale);
    expect(scaleNode != nullptr && scaleNode->payload.type == sdf3d::SdfNodeType::Scale, testName, "Expected Scale node type.", failures);

    bool hasChildLink = false;
    bool hasOutgoingLink = false;
    bool oldOutgoingRemoved = true;
    for (const sdf3d::SdfGraphLink& link : graph.links()) {
        hasChildLink = hasChildLink || (link.fromNode == sphere && link.fromSocket == "sdf" && link.toNode == scale && link.toSocket == "child");
        hasOutgoingLink = hasOutgoingLink || (link.fromNode == scale && link.fromSocket == "sdf" && link.toNode == material && link.toSocket == "sdf");
        if (link.fromNode == sphere && link.toNode == material) {
            oldOutgoingRemoved = false;
        }
    }
    expect(hasChildLink, testName, "Expected primitive linked into Scale child.", failures);
    expect(hasOutgoingLink, testName, "Expected Scale to feed old consumer.", failures);
    expect(oldOutgoingRemoved, testName, "Expected old primitive consumer link removed.", failures);
}

void testGraphSystemReusesScaleWrapper(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph system reuses scale wrapper";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId scale = graph.createNode(sdf3d::SdfNodeType::Scale, "Scale");
    graph.link(sphere, "sdf", scale, "child");

    const sdf3d::SdfGraphNodeId reused = sdf3d::GraphSystem::ensureScaleWrapperForNode(graph, sphere);

    expect(reused == scale, testName, "Expected existing Scale wrapper reused.", failures);
    expect(graph.selectedNode() == scale, testName, "Expected existing Scale selected.", failures);
    expect(graph.nodes().size() == 3, testName, "Expected no extra node beyond Output, Sphere, Scale.", failures);
}

void testGraphSystemAccumulatedTranslateDirectChain(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph system accumulated translate direct chain";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId first = graph.createNode(sdf3d::SdfNodeType::Translate, "T1");
    const sdf3d::SdfGraphNodeId second = graph.createNode(sdf3d::SdfNodeType::Translate, "T2");
    graph.node(first)->payload.parameters["x"] = 2.0f;
    graph.node(second)->payload.parameters["y"] = 3.0f;
    graph.link(sphere, "sdf", first, "child");
    graph.link(first, "sdf", second, "child");

    expectVec3(sdf3d::GraphSystem::accumulatedTranslatePosition(graph, second), {2.0f, 3.0f, 0.0f}, testName, "Expected direct Translate chain sum.", failures);
}

void testGraphSystemAccumulatedTranslateUnaryPassThrough(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph system accumulated translate unary pass through";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId first = graph.createNode(sdf3d::SdfNodeType::Translate, "T1");
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Mat");
    const sdf3d::SdfGraphNodeId second = graph.createNode(sdf3d::SdfNodeType::Translate, "T2");
    graph.node(first)->payload.parameters["x"] = 2.0f;
    graph.node(second)->payload.parameters["z"] = 4.0f;
    graph.link(sphere, "sdf", first, "child");
    graph.link(first, "sdf", material, "sdf");
    graph.link(material, "sdf", second, "child");

    expectVec3(sdf3d::GraphSystem::accumulatedTranslatePosition(graph, second), {2.0f, 0.0f, 4.0f}, testName, "Expected unary pass-through Translate sum.", failures);
}

void testGraphSystemAccumulatedTranslateStopsAtBranch(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph system accumulated translate stops at branch";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId first = graph.createNode(sdf3d::SdfNodeType::Translate, "T1");
    const sdf3d::SdfGraphNodeId box = graph.createNode(sdf3d::SdfNodeType::Box, "Box");
    const sdf3d::SdfGraphNodeId join = graph.createNode(sdf3d::SdfNodeType::Union, "Join");
    const sdf3d::SdfGraphNodeId second = graph.createNode(sdf3d::SdfNodeType::Translate, "T2");
    graph.node(first)->payload.parameters["x"] = 2.0f;
    graph.node(second)->payload.parameters["z"] = 4.0f;
    graph.link(sphere, "sdf", first, "child");
    graph.link(first, "sdf", join, "inputs");
    graph.link(box, "sdf", join, "inputs");
    graph.link(join, "sdf", second, "child");

    expectVec3(sdf3d::GraphSystem::accumulatedTranslatePosition(graph, second), {0.0f, 0.0f, 4.0f}, testName, "Expected accumulation to stop at multi-input branch.", failures);
}

void testGraphSystemPlacePrimitiveAtEmptyOutput(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph system place primitive at empty output";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = sdf3d::GraphSystem::placePrimitiveAtWorldPosition(graph, sphere, {1.0f, 2.0f, 3.0f});

    expect(translate != 0, testName, "Expected Translate wrapper.", failures);
    const sdf3d::SdfGraphNode* translateNode = graph.node(translate);
    expect(translateNode != nullptr && translateNode->payload.type == sdf3d::SdfNodeType::Translate, testName, "Expected Translate node.", failures);
    if (translateNode != nullptr) {
        expectVec3(sdf3d::GraphSystem::accumulatedTranslatePosition(graph, translate), {1.0f, 2.0f, 3.0f}, testName, "Expected placed world position.", failures);
    }

    bool translateFeedsOutput = false;
    for (const sdf3d::SdfGraphLink& link : graph.links()) {
        translateFeedsOutput = translateFeedsOutput || (link.fromNode == translate && link.toNode == graph.outputNode() && link.toSocket == "surface");
    }
    expect(translateFeedsOutput, testName, "Expected Translate linked to Output.", failures);
    expect(graph.selectedNode() == translate, testName, "Expected Translate selected.", failures);
}

void testGraphSystemPlacePrimitiveUnionsExistingOutput(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph system place primitive unions existing output";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId oldSphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Old");
    graph.link(oldSphere, "sdf", graph.outputNode(), "surface");
    const sdf3d::SdfGraphNodeId newBox = graph.createNode(sdf3d::SdfNodeType::Box, "New");
    const sdf3d::SdfGraphNodeId translate = sdf3d::GraphSystem::placePrimitiveAtWorldPosition(graph, newBox, {4.0f, 0.0f, 0.0f});

    sdf3d::SdfGraphNodeId unionNode = 0;
    for (const auto& [id, node] : graph.nodes()) {
        if (node.payload.type == sdf3d::SdfNodeType::Union) {
            unionNode = id;
            break;
        }
    }

    expect(unionNode != 0, testName, "Expected Union node.", failures);
    bool oldFeedsUnion = false;
    bool translateFeedsUnion = false;
    bool unionFeedsOutput = false;
    bool oldStillFeedsOutput = false;
    for (const sdf3d::SdfGraphLink& link : graph.links()) {
        oldFeedsUnion = oldFeedsUnion || (link.fromNode == oldSphere && link.toNode == unionNode && link.toSocket == "inputs");
        translateFeedsUnion = translateFeedsUnion || (link.fromNode == translate && link.toNode == unionNode && link.toSocket == "inputs");
        unionFeedsOutput = unionFeedsOutput || (link.fromNode == unionNode && link.toNode == graph.outputNode() && link.toSocket == "surface");
        oldStillFeedsOutput = oldStillFeedsOutput || (link.fromNode == oldSphere && link.toNode == graph.outputNode());
    }
    expect(oldFeedsUnion, testName, "Expected old root linked to Union.inputs.", failures);
    expect(translateFeedsUnion, testName, "Expected new Translate linked to Union.inputs.", failures);
    expect(unionFeedsOutput, testName, "Expected Union linked to Output.", failures);
    expect(!oldStillFeedsOutput, testName, "Expected old direct Output link removed.", failures);
    expect(graph.selectedNode() == translate, testName, "Expected new Translate selected.", failures);
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
        expect(repeatNode->payload.parameters.at("repeatX") == 1.0f, testName, "Expected repeat x enabled by default.", failures);
        expect(repeatNode->payload.parameters.at("repeatY") == 1.0f, testName, "Expected repeat y enabled by default.", failures);
        expect(repeatNode->payload.parameters.at("repeatZ") == 1.0f, testName, "Expected repeat z enabled by default.", failures);
        const sdf3d::SdfParameterDefinition* repeatX = sdf3d::sdfParameterDefinition(sdf3d::SdfNodeType::Repeat, "repeatX");
        const sdf3d::SdfParameterDefinition* repeatY = sdf3d::sdfParameterDefinition(sdf3d::SdfNodeType::Repeat, "repeatY");
        const sdf3d::SdfParameterDefinition* repeatZ = sdf3d::sdfParameterDefinition(sdf3d::SdfNodeType::Repeat, "repeatZ");
        expect(repeatX != nullptr && repeatX->type == sdf3d::SdfParameterType::Bool, testName, "Expected repeat x metadata to be bool.", failures);
        expect(repeatY != nullptr && repeatY->type == sdf3d::SdfParameterType::Bool, testName, "Expected repeat y metadata to be bool.", failures);
        expect(repeatZ != nullptr && repeatZ->type == sdf3d::SdfParameterType::Bool, testName, "Expected repeat z metadata to be bool.", failures);
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
        expect(twistNode->payload.parameters.at("axis") == 1.0f, testName, "Expected twist default y axis.", failures);
        const sdf3d::SdfParameterDefinition* twistAxis = sdf3d::sdfParameterDefinition(sdf3d::SdfNodeType::Twist, "axis");
        expect(twistAxis != nullptr && twistAxis->type == sdf3d::SdfParameterType::Enum, testName, "Expected twist axis metadata to be enum.", failures);
        expect(twistAxis != nullptr && twistAxis->enumValues.size() == 3, testName, "Expected twist axis enum values.", failures);
        if (twistAxis != nullptr && twistAxis->enumValues.size() == 3) {
            expect(twistAxis->enumValues[0].name == "X" && twistAxis->enumValues[0].value == 0, testName, "Expected twist X enum.", failures);
            expect(twistAxis->enumValues[1].name == "Y" && twistAxis->enumValues[1].value == 1, testName, "Expected twist Y enum.", failures);
            expect(twistAxis->enumValues[2].name == "Z" && twistAxis->enumValues[2].value == 2, testName, "Expected twist Z enum.", failures);
        }
    }
    if (bendNode != nullptr) {
        expect(bendNode->inputs.size() == 1 && bendNode->inputs[0].name == "child", testName, "Expected bend child input.", failures);
        expect(bendNode->outputs.size() == 1 && bendNode->outputs[0].name == "sdf", testName, "Expected bend SDF output.", failures);
        expect(bendNode->payload.parameters.at("strength") == 0.5f, testName, "Expected bend default strength.", failures);
        expect(bendNode->payload.parameters.at("axis") == 0.0f, testName, "Expected bend default x axis.", failures);
        const sdf3d::SdfParameterDefinition* bendAxis = sdf3d::sdfParameterDefinition(sdf3d::SdfNodeType::Bend, "axis");
        expect(bendAxis != nullptr && bendAxis->type == sdf3d::SdfParameterType::Enum, testName, "Expected bend axis metadata to be enum.", failures);
        expect(bendAxis != nullptr && bendAxis->enumValues.size() == 3, testName, "Expected bend axis enum values.", failures);
    }
}

void testNodeEditorAutoLayoutAllNodes(std::vector<TestFailure>& failures)
{
    const std::string testName = "node editor auto layout all nodes";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    graph.link(sphere, "sdf", material, "sdf");
    graph.link(material, "sdf", graph.outputNode(), "surface");

    if (sdf3d::SdfGraphNode* node = graph.node(sphere)) {
        node->editorX = 500.0f;
        node->editorY = 400.0f;
    }
    if (sdf3d::SdfGraphNode* node = graph.node(material)) {
        node->editorX = 100.0f;
        node->editorY = 800.0f;
    }
    if (sdf3d::SdfGraphNode* node = graph.node(graph.outputNode())) {
        node->editorX = 300.0f;
        node->editorY = 600.0f;
    }

    sdf3d::node_editor::CanvasFrame frame;
    frame.origin = {0.0f, 0.0f};
    frame.end = {1000.0f, 800.0f};
    frame.pan = {0.0f, 0.0f};
    frame.zoom = 1.0f;

    expect(sdf3d::node_editor::autoLayoutGraph(graph, frame, false), testName, "Expected layout to move nodes.", failures);

    const sdf3d::SdfGraphNode* sphereNode = graph.node(sphere);
    const sdf3d::SdfGraphNode* materialNode = graph.node(material);
    const sdf3d::SdfGraphNode* outputNode = graph.node(graph.outputNode());
    expect(sphereNode != nullptr && sphereNode->editorX == 116.0f, testName, "Expected leaf in first column.", failures);
    expect(materialNode != nullptr && materialNode->editorX == 476.0f, testName, "Expected material in second column.", failures);
    expect(outputNode != nullptr && outputNode->editorX == 836.0f, testName, "Expected output in third column.", failures);
    expect(sphereNode != nullptr && sphereNode->editorY == 298.0f, testName, "Expected sphere top aligned to shared first row.", failures);
    expect(materialNode != nullptr && materialNode->editorY == 298.0f, testName, "Expected material top aligned to shared first row.", failures);
    expect(outputNode != nullptr && outputNode->editorY == 298.0f, testName, "Expected output top aligned to shared first row.", failures);
}

void testNodeEditorAutoLayoutSelectedOnly(std::vector<TestFailure>& failures)
{
    const std::string testName = "node editor auto layout selected only";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    const sdf3d::SdfGraphNodeId box = graph.createNode(sdf3d::SdfNodeType::Box, "Box");
    graph.link(sphere, "sdf", material, "sdf");
    graph.link(material, "sdf", graph.outputNode(), "surface");

    if (sdf3d::SdfGraphNode* node = graph.node(sphere)) {
        node->editorX = 700.0f;
        node->editorY = 100.0f;
    }
    if (sdf3d::SdfGraphNode* node = graph.node(material)) {
        node->editorX = 100.0f;
        node->editorY = 300.0f;
    }
    if (sdf3d::SdfGraphNode* node = graph.node(box)) {
        node->editorX = 900.0f;
        node->editorY = 900.0f;
    }
    graph.setSelectedNodes({sphere, material}, material);

    sdf3d::node_editor::CanvasFrame frame;
    frame.origin = {0.0f, 0.0f};
    frame.end = {1000.0f, 800.0f};
    frame.pan = {0.0f, 0.0f};
    frame.zoom = 1.0f;

    expect(sdf3d::node_editor::autoLayoutGraph(graph, frame, true), testName, "Expected selected layout to move selected nodes.", failures);

    const sdf3d::SdfGraphNode* sphereNode = graph.node(sphere);
    const sdf3d::SdfGraphNode* materialNode = graph.node(material);
    const sdf3d::SdfGraphNode* boxNode = graph.node(box);
    expect(sphereNode != nullptr && sphereNode->editorX == 296.0f, testName, "Expected selected leaf in first column.", failures);
    expect(materialNode != nullptr && materialNode->editorX == 656.0f, testName, "Expected selected consumer in second column.", failures);
    expect(sphereNode != nullptr && sphereNode->editorY == 298.0f, testName, "Expected selected leaf top aligned to shared first row.", failures);
    expect(materialNode != nullptr && materialNode->editorY == 298.0f, testName, "Expected selected consumer top aligned to shared first row.", failures);
    expect(boxNode != nullptr && boxNode->editorX == 900.0f && boxNode->editorY == 900.0f, testName, "Expected unselected node unchanged.", failures);
}

void testNodeEditorAutoLayoutOrdersRowsByParentRow(std::vector<TestFailure>& failures)
{
    const std::string testName = "node editor auto layout orders rows by parent row";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId box = graph.createNode(sdf3d::SdfNodeType::Box, "Box");
    const sdf3d::SdfGraphNodeId boxMaterial = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Box Material");
    const sdf3d::SdfGraphNodeId sphereMaterial = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Sphere Material");

    graph.link(box, "sdf", boxMaterial, "sdf");
    graph.link(sphere, "sdf", sphereMaterial, "sdf");
    graph.link(sphereMaterial, "sdf", graph.outputNode(), "surface");

    sdf3d::node_editor::CanvasFrame frame;
    frame.origin = {0.0f, 0.0f};
    frame.end = {1000.0f, 800.0f};
    frame.pan = {0.0f, 0.0f};
    frame.zoom = 1.0f;

    expect(sdf3d::node_editor::autoLayoutGraph(graph, frame, false), testName, "Expected layout to move nodes.", failures);

    const sdf3d::SdfGraphNode* sphereNode = graph.node(sphere);
    const sdf3d::SdfGraphNode* boxNode = graph.node(box);
    const sdf3d::SdfGraphNode* boxMaterialNode = graph.node(boxMaterial);
    const sdf3d::SdfGraphNode* sphereMaterialNode = graph.node(sphereMaterial);

    expect(sphereNode != nullptr && boxNode != nullptr && sphereNode->editorY < boxNode->editorY, testName, "Expected lower-id leaf above second leaf.", failures);
    expect(sphereMaterialNode != nullptr && boxMaterialNode != nullptr && sphereMaterialNode->editorY < boxMaterialNode->editorY, testName, "Expected child of upper parent above child of lower parent.", failures);
}

void testNodeEditorAutoLayoutPreservesEmptyRowSlots(std::vector<TestFailure>& failures)
{
    const std::string testName = "node editor auto layout preserves empty row slots";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId source = graph.createNode(sdf3d::SdfNodeType::Sphere, "Source");
    const sdf3d::SdfGraphNodeId upper = graph.createNode(sdf3d::SdfNodeType::Translate, "Upper");
    const sdf3d::SdfGraphNodeId lower = graph.createNode(sdf3d::SdfNodeType::Translate, "Lower");
    const sdf3d::SdfGraphNodeId lowerNext = graph.createNode(sdf3d::SdfNodeType::Scale, "Lower Next");
    const sdf3d::SdfGraphNodeId join = graph.createNode(sdf3d::SdfNodeType::Union, "Join");

    graph.link(source, "sdf", upper, "child");
    graph.link(source, "sdf", lower, "child");
    graph.link(lower, "sdf", lowerNext, "child");
    graph.link(upper, "sdf", join, "inputs");
    graph.link(lowerNext, "sdf", join, "inputs");
    graph.setSelectedNodes({source, upper, lower, lowerNext, join}, join);

    sdf3d::node_editor::CanvasFrame frame;
    frame.origin = {0.0f, 0.0f};
    frame.end = {1000.0f, 800.0f};
    frame.pan = {0.0f, 0.0f};
    frame.zoom = 1.0f;

    expect(sdf3d::node_editor::autoLayoutGraph(graph, frame, true), testName, "Expected layout to move nodes.", failures);

    const sdf3d::SdfGraphNode* sourceNode = graph.node(source);
    const sdf3d::SdfGraphNode* upperNode = graph.node(upper);
    const sdf3d::SdfGraphNode* lowerNode = graph.node(lower);
    const sdf3d::SdfGraphNode* lowerNextNode = graph.node(lowerNext);
    const sdf3d::SdfGraphNode* joinNode = graph.node(join);

    expect(sourceNode != nullptr && upperNode != nullptr && lowerNode != nullptr && lowerNextNode != nullptr && joinNode != nullptr, testName, "Expected all layout nodes.", failures);
    if (sourceNode != nullptr && upperNode != nullptr && lowerNode != nullptr && lowerNextNode != nullptr && joinNode != nullptr) {
        expect(sourceNode->editorY == upperNode->editorY, testName, "Expected first child to share source row.", failures);
        expect(lowerNode->editorY > upperNode->editorY, testName, "Expected second child below first child.", failures);
        expect(lowerNextNode->editorY == lowerNode->editorY, testName, "Expected lower chain to preserve row through empty slot.", failures);
        expect(joinNode->editorY == upperNode->editorY, testName, "Expected join to stay on first input row.", failures);
        expect(sourceNode->editorX < lowerNode->editorX && lowerNode->editorX < lowerNextNode->editorX && lowerNextNode->editorX < joinNode->editorX, testName, "Expected longer branch to flow left-to-right.", failures);
        expect(upperNode->editorX == lowerNextNode->editorX, testName, "Expected nodes at same reverse depth to share a column.", failures);
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
    testGraphSystemCreatesTranslateWrapper(failures);
    testGraphSystemReusesTranslateWrapper(failures);
    testGraphSystemCreatesRotateWrapper(failures);
    testGraphSystemReusesRotateWrapper(failures);
    testRotateQuaternionParamsAreHiddenMetadata(failures);
    testMaterialOverrideGetsRegistryMaterial(failures);
    testGraphSystemCreatesScaleWrapper(failures);
    testGraphSystemReusesScaleWrapper(failures);
    testGraphSystemAccumulatedTranslateDirectChain(failures);
    testGraphSystemAccumulatedTranslateUnaryPassThrough(failures);
    testGraphSystemAccumulatedTranslateStopsAtBranch(failures);
    testGraphSystemPlacePrimitiveAtEmptyOutput(failures);
    testGraphSystemPlacePrimitiveUnionsExistingOutput(failures);
    testPhaseTwoDomainNodeDefinitions(failures);
    testNodeEditorAutoLayoutAllNodes(failures);
    testNodeEditorAutoLayoutSelectedOnly(failures);
    testNodeEditorAutoLayoutOrdersRowsByParentRow(failures);
    testNodeEditorAutoLayoutPreservesEmptyRowSlots(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All SDF graph tests passed.\n";
    return 0;
}
