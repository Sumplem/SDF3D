#include "sdf3d/core/EventBus.h"
#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/systems/GraphSystem.h"

#include <algorithm>
#include <iostream>
#include <optional>
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
    expect(graph.link(material, "sdf", unionNode, "inputs"), testName, "Expected material to union link.", failures);
    expect(graph.link(box, "sdf", unionNode, "inputs"), testName, "Expected external box to union link.", failures);
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
    expect(hasLink(graph, materialCopy, "sdf", unionCopy, "inputs"), testName, "Expected copied material to union link.", failures);
    expect(!hasLink(graph, box, "sdf", unionCopy, "inputs"), testName, "Expected external incoming link to be dropped.", failures);
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

    expect(graph.link(sphere, "sdf", unionNode, "inputs"), testName, "Expected valid input link.", failures);
    expect(graph.link(emptyTranslate, "sdf", unionNode, "inputs"), testName, "Expected invalid input link.", failures);

    expect(sdf3d::GraphSystem::producesValidSdf(graph, sphere), testName, "Expected primitive to produce valid SDF.", failures);
    expect(!sdf3d::GraphSystem::producesValidSdf(graph, emptyTranslate), testName, "Expected transform without child to be invalid.", failures);
    expect(sdf3d::GraphSystem::producesValidSdf(graph, unionNode), testName, "Expected union to bypass invalid input.", failures);
    expect(sdf3d::GraphSystem::effectiveLinksToInput(graph, unionNode, "inputs").size() == 1, testName, "Expected only valid effective input.", failures);

    const sdf3d::SdfGraphNode* unionGraphNode = graph.node(unionNode);
    expect(unionGraphNode != nullptr && sdf3d::GraphSystem::nodeHasMissingRequiredInput(graph, *unionGraphNode), testName, "Expected UI missing-input query to flag bypass visual.", failures);
    const std::optional<sdf3d::SdfGraphLink> bypass = unionGraphNode != nullptr
        ? sdf3d::GraphSystem::effectiveBypassSourceLink(graph, *unionGraphNode)
        : std::nullopt;
    expect(bypass.has_value() && bypass->fromNode == sphere && bypass->toSocket == "inputs", testName, "Expected shared bypass query to return valid source link.", failures);
}

void testMultiInputSocketKeepsMultipleLinks(std::vector<TestFailure>& failures)
{
    const std::string testName = "multi input socket keeps multiple links";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId box = graph.createNode(sdf3d::SdfNodeType::Box, "Box");
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");

    expect(graph.link(sphere, "sdf", unionNode, "inputs"), testName, "Expected first multi-input link.", failures);
    expect(graph.link(box, "sdf", unionNode, "inputs"), testName, "Expected second multi-input link.", failures);
    expect(!graph.link(box, "sdf", unionNode, "inputs"), testName, "Expected duplicate multi-input link rejected.", failures);

    std::size_t inputLinks = 0;
    for (const sdf3d::SdfGraphLink& link : graph.links()) {
        if (link.toNode == unionNode && link.toSocket == "inputs") {
            ++inputLinks;
        }
    }

    expect(inputLinks == 2, testName, "Expected exactly two links on one multi-input socket.", failures);
    expect(sdf3d::GraphSystem::effectiveLinksToInput(graph, unionNode, "inputs").size() == 2, testName, "Expected two effective multi-input links.", failures);
}

void testLoweredRequiredInputRules(std::vector<TestFailure>& failures)
{
    const std::string testName = "lowered required input rules";

    expect(sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::Union, {"inputs"}, 1), testName, "Expected single-input union valid.", failures);
    expect(sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::Subtract, {"base"}, 1), testName, "Expected subtract with base valid.", failures);
    expect(!sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::Translate, {}, 0), testName, "Expected transform without child invalid.", failures);
    expect(!sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::MaterialOverride, {}, 0), testName, "Expected material override without sdf invalid.", failures);
}

void testChangeMultiInputBooleanTypePreservesLinks(std::vector<TestFailure>& failures)
{
    const std::string testName = "change multi-input boolean type preserves links";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId box = graph.createNode(sdf3d::SdfNodeType::Box, "Box");
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Merge");
    expect(graph.link(sphere, "sdf", unionNode, "inputs"), testName, "Expected first input link.", failures);
    expect(graph.link(box, "sdf", unionNode, "inputs"), testName, "Expected second input link.", failures);

    expect(sdf3d::GraphSystem::changeNodeType(graph, unionNode, sdf3d::SdfNodeType::SmoothIntersect), testName, "Expected compatible type change.", failures);
    const sdf3d::SdfGraphNode* node = graph.node(unionNode);
    expect(node != nullptr && node->payload.type == sdf3d::SdfNodeType::SmoothIntersect, testName, "Expected SmoothIntersect type.", failures);
    expect(node != nullptr && node->payload.name == "Merge", testName, "Expected name preserved.", failures);
    expect(node != nullptr && node->payload.parameters.find("smoothness") != node->payload.parameters.end(), testName, "Expected smoothness param.", failures);

    std::size_t inputLinks = 0;
    for (const sdf3d::SdfGraphLink& link : graph.links()) {
        if (link.toNode == unionNode && link.toSocket == "inputs") {
            ++inputLinks;
        }
    }
    expect(inputLinks == 2, testName, "Expected input links preserved.", failures);

    expect(sdf3d::GraphSystem::changeNodeType(graph, unionNode, sdf3d::SdfNodeType::Intersect), testName, "Expected compatible non-smooth change.", failures);
    node = graph.node(unionNode);
    expect(node != nullptr && node->payload.type == sdf3d::SdfNodeType::Intersect, testName, "Expected Intersect type.", failures);
    expect(node != nullptr && node->payload.parameters.find("smoothness") == node->payload.parameters.end(), testName, "Expected smoothness removed.", failures);
    expect(!sdf3d::GraphSystem::changeNodeType(graph, unionNode, sdf3d::SdfNodeType::Subtract), testName, "Expected incompatible socket family rejected.", failures);
}

void testMaterialRegistryRenameAndSafeDelete(std::vector<TestFailure>& failures)
{
    const std::string testName = "material registry rename and safe delete";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId materialNodeId = graph.createNode(sdf3d::SdfNodeType::SolidMaterial, "Paint");
    sdf3d::SdfGraphNode* materialNode = graph.node(materialNodeId);
    expect(materialNode != nullptr && materialNode->payload.materialId != 0, testName, "Expected material node with registry id.", failures);
    if (materialNode == nullptr) {
        return;
    }

    const sdf3d::MaterialId referencedId = materialNode->payload.materialId;
    expect(sdf3d::GraphSystem::renameMaterial(graph, referencedId, "Renamed Paint"), testName, "Expected material rename.", failures);
    expect(materialNode->payload.name == "Renamed Paint", testName, "Expected material node name to follow registry rename.", failures);
    const sdf3d::MaterialDefinition* renamed = graph.materials().material(referencedId);
    expect(renamed != nullptr && renamed->name == "Renamed Paint", testName, "Expected registry name updated.", failures);

    expect(!sdf3d::GraphSystem::canDeleteMaterial(graph, referencedId), testName, "Expected referenced material delete blocked.", failures);
    expect(!sdf3d::GraphSystem::deleteMaterial(graph, referencedId), testName, "Expected referenced material delete to fail.", failures);
    expect(graph.materials().material(referencedId) != nullptr, testName, "Expected referenced material preserved.", failures);

    const sdf3d::MaterialId orphanId = graph.materials().createMaterial("Orphan");
    expect(sdf3d::GraphSystem::canDeleteMaterial(graph, orphanId), testName, "Expected orphan material deletable.", failures);
    expect(sdf3d::GraphSystem::deleteMaterial(graph, orphanId), testName, "Expected orphan material delete.", failures);
    expect(graph.materials().material(orphanId) == nullptr, testName, "Expected orphan material removed.", failures);
}

void testMaterialSourceDeleteCleansUnreferencedRegistryEntry(std::vector<TestFailure>& failures)
{
    const std::string testName = "material source delete cleans unreferenced registry entry";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId materialNodeId = graph.createNode(sdf3d::SdfNodeType::CheckerMaterial, "Paint");
    const sdf3d::SdfGraphNode* materialNode = graph.node(materialNodeId);
    expect(materialNode != nullptr && materialNode->payload.materialId != 0, testName, "Expected material node with registry id.", failures);
    if (materialNode == nullptr) {
        return;
    }

    const sdf3d::MaterialId materialId = materialNode->payload.materialId;
    expect(graph.materials().material(materialId) != nullptr, testName, "Expected registry material before delete.", failures);
    expect(graph.deleteNode(materialNodeId), testName, "Expected material node delete.", failures);
    expect(graph.materials().material(materialId) == nullptr, testName, "Expected unreferenced registry material removed.", failures);
}

void testMaterialSourceDeleteKeepsRegistryEntryReferencedByOverride(std::vector<TestFailure>& failures)
{
    const std::string testName = "material source delete keeps registry entry referenced by override";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId materialNodeId = graph.createNode(sdf3d::SdfNodeType::SolidMaterial, "Paint");
    const sdf3d::SdfGraphNodeId linkedOverrideNodeId = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Apply Paint");
    const sdf3d::SdfGraphNodeId otherOverrideNodeId = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Other Paint");
    const sdf3d::SdfGraphNode* materialNode = graph.node(materialNodeId);
    expect(materialNode != nullptr && materialNode->payload.materialId != 0, testName, "Expected material node with registry id.", failures);
    if (materialNode == nullptr) {
        return;
    }

    const sdf3d::MaterialId materialId = materialNode->payload.materialId;
    if (sdf3d::SdfGraphNode* otherOverride = graph.node(otherOverrideNodeId)) {
        otherOverride->payload.materialId = materialId;
    }
    expect(graph.link(materialNodeId, "material", linkedOverrideNodeId, "material"), testName, "Expected material link.", failures);
    const sdf3d::SdfGraphNode* linkedOverride = graph.node(linkedOverrideNodeId);
    expect(linkedOverride != nullptr && linkedOverride->payload.materialId == materialId, testName, "Expected linked override to reference material id.", failures);
    expect(graph.deleteNode(materialNodeId), testName, "Expected material node delete.", failures);
    expect(graph.materials().material(materialId) != nullptr, testName, "Expected referenced registry material preserved.", failures);
    linkedOverride = graph.node(linkedOverrideNodeId);
    const sdf3d::SdfGraphNode* otherOverride = graph.node(otherOverrideNodeId);
    expect(linkedOverride != nullptr && linkedOverride->payload.materialId == 0, testName, "Expected directly linked override cleared.", failures);
    expect(otherOverride != nullptr && otherOverride->payload.materialId == materialId, testName, "Expected other override still referencing material.", failures);
    expect(graph.deleteNode(otherOverrideNodeId), testName, "Expected other override delete.", failures);
    expect(graph.materials().material(materialId) == nullptr, testName, "Expected unreferenced registry material removed.", failures);
}

void testMaterialSourceDeleteClearsDirectOverrideReference(std::vector<TestFailure>& failures)
{
    const std::string testName = "material source delete clears direct override reference";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId materialNodeId = graph.createNode(sdf3d::SdfNodeType::SolidMaterial, "Paint");
    const sdf3d::SdfGraphNodeId overrideNodeId = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Apply Paint");
    const sdf3d::SdfGraphNode* materialNode = graph.node(materialNodeId);
    expect(materialNode != nullptr && materialNode->payload.materialId != 0, testName, "Expected material node with registry id.", failures);
    if (materialNode == nullptr) {
        return;
    }

    const sdf3d::MaterialId materialId = materialNode->payload.materialId;
    expect(graph.link(materialNodeId, "material", overrideNodeId, "material"), testName, "Expected material link.", failures);
    expect(graph.deleteNode(materialNodeId), testName, "Expected material node delete.", failures);
    const sdf3d::SdfGraphNode* overrideNode = graph.node(overrideNodeId);
    expect(overrideNode != nullptr && overrideNode->payload.materialId == 0, testName, "Expected override reset to default material.", failures);
    expect(graph.materials().material(materialId) == nullptr, testName, "Expected registry material removed.", failures);
}

void testUnlinkMaterialInputClearsOverrideMaterial(std::vector<TestFailure>& failures)
{
    const std::string testName = "unlink material input clears override material";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId materialNodeId = graph.createNode(sdf3d::SdfNodeType::SolidMaterial, "Paint");
    const sdf3d::SdfGraphNodeId overrideNodeId = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Apply Paint");
    const sdf3d::SdfGraphNode* materialNode = graph.node(materialNodeId);
    const sdf3d::MaterialId materialId = materialNode != nullptr ? materialNode->payload.materialId : 0;
    expect(graph.link(materialNodeId, "material", overrideNodeId, "material"), testName, "Expected material link.", failures);
    const sdf3d::SdfGraphNode* linkedOverride = graph.node(overrideNodeId);
    expect(linkedOverride != nullptr && linkedOverride->payload.materialId == materialId, testName, "Expected linked override material.", failures);

    expect(graph.unlink(materialNodeId, "material", overrideNodeId, "material"), testName, "Expected material unlink.", failures);
    const sdf3d::SdfGraphNode* unlinkedOverride = graph.node(overrideNodeId);
    expect(unlinkedOverride != nullptr && unlinkedOverride->payload.materialId == 0, testName, "Expected override to use default material.", failures);
}

void testCollectNodeParamsPacksTransformValues(std::vector<TestFailure>& failures)
{
    const std::string testName = "collect node params packs transform values";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    if (sdf3d::SdfGraphNode* node = graph.node(translate)) {
        node->payload.parameters["x"] = 1.0f;
        node->payload.parameters["y"] = 2.0f;
        node->payload.parameters["z"] = 3.0f;
    }
    const sdf3d::SdfGraphNodeId rotate = graph.createNode(sdf3d::SdfNodeType::Rotate, "Rotate");
    if (sdf3d::SdfGraphNode* node = graph.node(rotate)) {
        sdf3d::storeRotationQuaternion(node->payload, {0.0f, 0.0f, 0.70710677f, 0.70710677f});
    }
    const sdf3d::SdfGraphNodeId scale = graph.createNode(sdf3d::SdfNodeType::Scale, "Scale");
    if (sdf3d::SdfGraphNode* node = graph.node(scale)) {
        node->payload.parameters["x"] = 2.0f;
        node->payload.parameters["y"] = 3.0f;
        node->payload.parameters["z"] = 4.0f;
    }

    const std::vector<sdf3d::SdfCompiledNodeParam> params = sdf3d::GraphSystem::collectNodeParams(graph);

    expect(params.size() == 3, testName, "Expected three transform param entries.", failures);
    const auto translateIt = std::find_if(params.begin(), params.end(), [translate](const sdf3d::SdfCompiledNodeParam& param) {
        return param.nodeId == translate;
    });
    const auto rotateIt = std::find_if(params.begin(), params.end(), [rotate](const sdf3d::SdfCompiledNodeParam& param) {
        return param.nodeId == rotate;
    });
    const auto scaleIt = std::find_if(params.begin(), params.end(), [scale](const sdf3d::SdfCompiledNodeParam& param) {
        return param.nodeId == scale;
    });
    if (translateIt != params.end()) {
        expect(translateIt->data0[0] == 1.0f, testName, "Expected x packed.", failures);
        expect(translateIt->data0[1] == 2.0f, testName, "Expected y packed.", failures);
        expect(translateIt->data0[2] == 3.0f, testName, "Expected z packed.", failures);
    } else {
        expect(false, testName, "Expected translate node id packed.", failures);
    }
    if (rotateIt != params.end()) {
        expect(rotateIt->data0[0] == 0.0f, testName, "Expected qx packed.", failures);
        expect(rotateIt->data0[1] == 0.0f, testName, "Expected qy packed.", failures);
        expect(rotateIt->data0[2] > 0.7f && rotateIt->data0[2] < 0.71f, testName, "Expected qz packed.", failures);
        expect(rotateIt->data0[3] > 0.7f && rotateIt->data0[3] < 0.71f, testName, "Expected qw packed.", failures);
    } else {
        expect(false, testName, "Expected rotate node id packed.", failures);
    }
    if (scaleIt != params.end()) {
        expect(scaleIt->data0[0] == 2.0f, testName, "Expected scale x packed.", failures);
        expect(scaleIt->data0[1] == 3.0f, testName, "Expected scale y packed.", failures);
        expect(scaleIt->data0[2] == 4.0f, testName, "Expected scale z packed.", failures);
        expect(scaleIt->data0[3] == 2.0f, testName, "Expected scale min axis packed.", failures);
    } else {
        expect(false, testName, "Expected scale node id packed.", failures);
    }
}

void testCollectNodeParamsNormalizesRotateQuaternion(std::vector<TestFailure>& failures)
{
    const std::string testName = "collect node params normalizes rotate quaternion";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId rotate = graph.createNode(sdf3d::SdfNodeType::Rotate, "Rotate");
    if (sdf3d::SdfGraphNode* node = graph.node(rotate)) {
        node->payload.parameters[sdf3d::RotateParamQx] = 0.0f;
        node->payload.parameters[sdf3d::RotateParamQy] = 0.0f;
        node->payload.parameters[sdf3d::RotateParamQz] = 0.0f;
        node->payload.parameters[sdf3d::RotateParamQw] = 2.0f;
    }

    const std::vector<sdf3d::SdfCompiledNodeParam> params = sdf3d::GraphSystem::collectNodeParams(graph);
    const auto rotateIt = std::find_if(params.begin(), params.end(), [rotate](const sdf3d::SdfCompiledNodeParam& param) {
        return param.nodeId == rotate;
    });

    expect(rotateIt != params.end(), testName, "Expected rotate node id packed.", failures);
    if (rotateIt != params.end()) {
        expect(rotateIt->data0[0] == 0.0f, testName, "Expected qx normalized.", failures);
        expect(rotateIt->data0[1] == 0.0f, testName, "Expected qy normalized.", failures);
        expect(rotateIt->data0[2] == 0.0f, testName, "Expected qz normalized.", failures);
        expect(rotateIt->data0[3] == 1.0f, testName, "Expected qw normalized.", failures);
    }
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
    expect(sdf3d::GraphSystem::highlightNodeForNode(graph, sphere) == translate, testName, "Expected picked primitive to highlight transform wrapper.", failures);

    graph.setSelectedNode(translate);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == translate, testName, "Expected transform selection to highlight itself.", failures);
    expect(sdf3d::GraphSystem::highlightNodeForNode(graph, translate) == translate, testName, "Expected picked transform to highlight itself.", failures);
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
    expect(sdf3d::GraphSystem::highlightNodeForNode(graph, sphere) == scale, testName, "Expected picked primitive to highlight final transform in chain.", failures);

    graph.setSelectedNode(translate);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == translate, testName, "Expected inner transform selection to highlight itself.", failures);

    graph.setSelectedNode(rotate);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == rotate, testName, "Expected middle transform selection to highlight itself.", failures);

    graph.setSelectedNode(scale);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == scale, testName, "Expected top transform selection to highlight itself.", failures);
}

void testHighlightNodeForSelectionStopsAtBranchedTransform(std::vector<TestFailure>& failures)
{
    const std::string testName = "highlight node for selection stops at branched transform";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId firstTranslate = graph.createNode(sdf3d::SdfNodeType::Translate, "Trans1");
    const sdf3d::SdfGraphNodeId secondTranslate = graph.createNode(sdf3d::SdfNodeType::Translate, "Trans2");
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");
    expect(graph.link(sphere, "sdf", firstTranslate, "child"), testName, "Expected sphere to first translate link.", failures);
    expect(graph.link(firstTranslate, "sdf", unionNode, "inputs"), testName, "Expected first translate to union input link.", failures);
    expect(graph.link(firstTranslate, "sdf", secondTranslate, "child"), testName, "Expected first translate to second translate link.", failures);
    expect(graph.link(secondTranslate, "sdf", unionNode, "inputs"), testName, "Expected second translate to union input link.", failures);
    expect(graph.link(unionNode, "sdf", graph.outputNode(), "surface"), testName, "Expected union to output link.", failures);

    graph.setSelectedNode(firstTranslate);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == firstTranslate, testName, "Expected selected transform to highlight itself.", failures);

    graph.setSelectedNode(sphere);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == firstTranslate, testName, "Expected primitive selection to stop at branched transform.", failures);
    expect(sdf3d::GraphSystem::highlightNodeForNode(graph, sphere) == firstTranslate, testName, "Expected picked primitive to stop at branched transform.", failures);
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
    expect(hasLink(graph, sphere, "sdf", rotate, "child"), testName, "Expected rotate inserted below translate.", failures);
    expect(hasLink(graph, rotate, "sdf", translate, "child"), testName, "Expected rotate to feed translate.", failures);
    expect(hasLink(graph, translate, "sdf", graph.outputNode(), "surface"), testName, "Expected translate to feed output.", failures);
    expect(graph.selectedNode() == rotate, testName, "Expected new rotate wrapper selected.", failures);
}

void testEnsureTransformWrapperUsesCanonicalOrder(std::vector<TestFailure>& failures)
{
    const std::string testName = "ensure transform wrapper uses canonical order";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    expect(graph.link(sphere, "sdf", translate, "child"), testName, "Expected sphere to translate link.", failures);
    expect(graph.link(translate, "sdf", graph.outputNode(), "surface"), testName, "Expected translate to output link.", failures);

    const sdf3d::SdfGraphNodeId scale = sdf3d::GraphSystem::ensureScaleWrapperForNode(graph, translate);
    const sdf3d::SdfGraphNodeId rotate = sdf3d::GraphSystem::ensureRotateWrapperForNode(graph, translate);

    expect(scale != 0, testName, "Expected scale wrapper.", failures);
    expect(rotate != 0, testName, "Expected rotate wrapper.", failures);
    expect(hasLink(graph, sphere, "sdf", scale, "child"), testName, "Expected scale below rotate/translate.", failures);
    expect(hasLink(graph, scale, "sdf", rotate, "child"), testName, "Expected scale to feed rotate.", failures);
    expect(hasLink(graph, rotate, "sdf", translate, "child"), testName, "Expected rotate to feed translate.", failures);
    expect(hasLink(graph, translate, "sdf", graph.outputNode(), "surface"), testName, "Expected translate to stay outer transform.", failures);
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

void testEnsureTransformWrapperReusesExistingChainChild(std::vector<TestFailure>& failures)
{
    const std::string testName = "ensure transform wrapper reuses existing chain child";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId rotate = graph.createNode(sdf3d::SdfNodeType::Rotate, "Rotate");
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    expect(graph.link(sphere, "sdf", rotate, "child"), testName, "Expected sphere to rotate link.", failures);
    expect(graph.link(rotate, "sdf", translate, "child"), testName, "Expected rotate to translate link.", failures);
    expect(graph.link(translate, "sdf", graph.outputNode(), "surface"), testName, "Expected translate to output link.", failures);

    const sdf3d::SdfGraphNodeId reused = sdf3d::GraphSystem::ensureRotateWrapperForNode(graph, translate);

    expect(reused == rotate, testName, "Expected selected outer transform to reuse existing upstream rotate.", failures);
    expect(graph.links().size() == 3, testName, "Expected no duplicate wrapper links.", failures);
    expect(graph.selectedNode() == rotate, testName, "Expected existing rotate selected.", failures);
}

void testGroupSelectionCreatesDefinitionAndInstance(std::vector<TestFailure>& failures)
{
    const std::string testName = "group selection creates definition and instance";
    sdf3d::SdfGraph graph;
    sdf3d::GraphGroupRegistry groups;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    expect(graph.link(sphere, "sdf", graph.outputNode(), "surface"), testName, "Expected sphere to output link.", failures);
    expect(graph.setSelectedNodes({sphere}, sphere), testName, "Expected sphere selection.", failures);

    const sdf3d::SdfGraphNodeId group = sdf3d::GraphSystem::groupSelection(graph, groups, graph.selectedNodes(), graph.selectedNode(), "Sphere Group");

    expect(group != 0, testName, "Expected group instance.", failures);
    const sdf3d::SdfGraphNode* groupNode = graph.node(group);
    expect(groupNode != nullptr && groupNode->payload.type == sdf3d::SdfNodeType::Group, testName, "Expected Group node type.", failures);
    expect(groupNode != nullptr && groupNode->payload.groupDefinitionId != 0, testName, "Expected group definition id.", failures);
    expect(groups.definitions().size() == 1, testName, "Expected one registry definition.", failures);
    expect(graph.node(sphere) == nullptr, testName, "Expected grouped source removed from instance graph.", failures);
    expect(hasLink(graph, group, "sdf", graph.outputNode(), "surface"), testName, "Expected group linked to output.", failures);
}

void testGroupSelectionUsesOutgoingSurfaceRoot(std::vector<TestFailure>& failures)
{
    const std::string testName = "group selection uses outgoing surface root";
    sdf3d::SdfGraph graph;
    sdf3d::GraphGroupRegistry groups;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId overrideNode = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Override");
    expect(graph.link(sphere, "sdf", overrideNode, "sdf"), testName, "Expected sphere to override link.", failures);
    expect(graph.link(overrideNode, "sdf", graph.outputNode(), "surface"), testName, "Expected override to output link.", failures);
    expect(graph.setSelectedNodes({sphere, overrideNode}, sphere), testName, "Expected selection with inner primary.", failures);

    const sdf3d::SdfGraphNodeId group = sdf3d::GraphSystem::groupSelection(graph, groups, graph.selectedNodes(), graph.selectedNode(), "Surface Group");

    expect(group != 0, testName, "Expected group instance.", failures);
    expect(hasLink(graph, group, "sdf", graph.outputNode(), "surface"), testName, "Expected group output link restored from selected surface root.", failures);
    const sdf3d::GraphGroupDefinition* definition = groups.definitions().empty() ? nullptr : &groups.definitions().front();
    expect(definition != nullptr, testName, "Expected group definition.", failures);
    if (definition != nullptr) {
        expect(hasLink(definition->subgraph, overrideNode, "sdf", definition->subgraph.outputNode(), "surface"), testName, "Expected definition output to use surface root, not primary.", failures);
    }
}

void testGroupSelectionRejectsMultipleExternalOutputs(std::vector<TestFailure>& failures)
{
    const std::string testName = "group selection rejects multiple external outputs";
    sdf3d::SdfGraph graph;
    sdf3d::GraphGroupRegistry groups;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId box = graph.createNode(sdf3d::SdfNodeType::Box, "Box");
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");
    expect(graph.link(sphere, "sdf", unionNode, "inputs"), testName, "Expected sphere external output.", failures);
    expect(graph.link(box, "sdf", unionNode, "inputs"), testName, "Expected box external output.", failures);
    expect(graph.link(unionNode, "sdf", graph.outputNode(), "surface"), testName, "Expected union output link.", failures);
    expect(graph.setSelectedNodes({sphere, box}, sphere), testName, "Expected selection with two external outputs.", failures);

    const sdf3d::SdfGraphNodeId group = sdf3d::GraphSystem::groupSelection(graph, groups, graph.selectedNodes(), graph.selectedNode(), "Invalid Group");

    expect(group == 0, testName, "Expected group creation rejected.", failures);
    expect(groups.definitions().empty(), testName, "Expected no definition created.", failures);
    expect(graph.node(sphere) != nullptr, testName, "Expected sphere preserved.", failures);
    expect(graph.node(box) != nullptr, testName, "Expected box preserved.", failures);
    expect(hasLink(graph, sphere, "sdf", unionNode, "inputs"), testName, "Expected sphere link preserved.", failures);
    expect(hasLink(graph, box, "sdf", unionNode, "inputs"), testName, "Expected box link preserved.", failures);
}

void testRenameGroupNodeUpdatesDefinitionName(std::vector<TestFailure>& failures)
{
    const std::string testName = "rename group node updates definition name";
    sdf3d::SdfGraph graph;
    sdf3d::GraphGroupRegistry groups;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    expect(graph.link(sphere, "sdf", graph.outputNode(), "surface"), testName, "Expected sphere to output link.", failures);
    expect(graph.setSelectedNodes({sphere}, sphere), testName, "Expected sphere selection.", failures);

    const sdf3d::SdfGraphNodeId group = sdf3d::GraphSystem::groupSelection(graph, groups, graph.selectedNodes(), graph.selectedNode(), "Old Group");
    expect(group != 0, testName, "Expected group instance.", failures);
    expect(sdf3d::GraphSystem::renameNode(graph, groups, group, "New Group"), testName, "Expected group rename.", failures);

    const sdf3d::SdfGraphNode* groupNode = graph.node(group);
    const sdf3d::GraphGroupDefinition* definition = groups.definitions().empty() ? nullptr : &groups.definitions().front();
    expect(groupNode != nullptr && groupNode->payload.name == "New Group", testName, "Expected group node name updated.", failures);
    expect(definition != nullptr && definition->name == "New Group", testName, "Expected definition name updated.", failures);
    if (groupNode != nullptr) {
        expect(sdf3d::GraphSystem::displayNameForNode(*groupNode, groups) == "New Group", testName, "Expected display name to resolve through definition.", failures);
    }
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testDuplicateSelectionCopiesInternalLinksOnly(failures);
    testDuplicateSelectionSkipsOutput(failures);
    testEffectiveValidityDropsInvalidUpstream(failures);
    testMultiInputSocketKeepsMultipleLinks(failures);
    testLoweredRequiredInputRules(failures);
    testChangeMultiInputBooleanTypePreservesLinks(failures);
    testMaterialRegistryRenameAndSafeDelete(failures);
    testMaterialSourceDeleteCleansUnreferencedRegistryEntry(failures);
    testMaterialSourceDeleteKeepsRegistryEntryReferencedByOverride(failures);
    testMaterialSourceDeleteClearsDirectOverrideReference(failures);
    testUnlinkMaterialInputClearsOverrideMaterial(failures);
    testCollectNodeParamsPacksTransformValues(failures);
    testCollectNodeParamsNormalizesRotateQuaternion(failures);
    testHighlightNodeForSelectionUsesTransformWrapper(failures);
    testHighlightNodeForSelectionFollowsTransformChain(failures);
    testHighlightNodeForSelectionStopsAtBranchedTransform(failures);
    testEnsureTransformWrapperCanChainFromTransform(failures);
    testEnsureTransformWrapperUsesCanonicalOrder(failures);
    testEnsureTransformWrapperReusesExistingChainParent(failures);
    testEnsureTransformWrapperReusesExistingChainChild(failures);
    testGroupSelectionCreatesDefinitionAndInstance(failures);
    testGroupSelectionUsesOutgoingSurfaceRoot(failures);
    testGroupSelectionRejectsMultipleExternalOutputs(failures);
    testRenameGroupNodeUpdatesDefinitionName(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All GraphSystem tests passed.\n";
    return 0;
}
