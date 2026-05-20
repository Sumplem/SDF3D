#include "sdf3d/core/EventBus.h"
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
    const std::optional<sdf3d::SdfGraphLink> bypass = unionGraphNode != nullptr
        ? sdf3d::GraphSystem::effectiveBypassSourceLink(graph, *unionGraphNode)
        : std::nullopt;
    expect(bypass.has_value() && bypass->fromNode == sphere && bypass->toSocket == "left", testName, "Expected shared bypass query to return valid source link.", failures);
}

void testLoweredRequiredInputRules(std::vector<TestFailure>& failures)
{
    const std::string testName = "lowered required input rules";

    expect(sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::Union, {"left"}, 1), testName, "Expected single-input union valid.", failures);
    expect(sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::Subtract, {"base"}, 1), testName, "Expected subtract with base valid.", failures);
    expect(!sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::Translate, {}, 0), testName, "Expected transform without child invalid.", failures);
    expect(!sdf3d::GraphSystem::loweredNodeHasRequiredInputs(sdf3d::SdfNodeType::MaterialOverride, {}, 0), testName, "Expected material override without sdf invalid.", failures);
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
    const sdf3d::SdfGraphNodeId overrideNodeId = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Apply Paint");
    const sdf3d::SdfGraphNode* materialNode = graph.node(materialNodeId);
    expect(materialNode != nullptr && materialNode->payload.materialId != 0, testName, "Expected material node with registry id.", failures);
    if (materialNode == nullptr) {
        return;
    }

    const sdf3d::MaterialId materialId = materialNode->payload.materialId;
    expect(graph.link(materialNodeId, "material", overrideNodeId, "material"), testName, "Expected material link.", failures);
    const sdf3d::SdfGraphNode* overrideNode = graph.node(overrideNodeId);
    expect(overrideNode != nullptr && overrideNode->payload.materialId == materialId, testName, "Expected override to reference material id.", failures);
    expect(graph.deleteNode(materialNodeId), testName, "Expected material node delete.", failures);
    expect(graph.materials().material(materialId) != nullptr, testName, "Expected referenced registry material preserved.", failures);
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

void testPickNodeByRaySelectsBooleanInputBranchTransform(std::vector<TestFailure>& failures)
{
    const std::string testName = "pick node by ray selects boolean input branch transform";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId firstTranslate = graph.createNode(sdf3d::SdfNodeType::Translate, "Trans1");
    const sdf3d::SdfGraphNodeId secondTranslate = graph.createNode(sdf3d::SdfNodeType::Translate, "Trans2");
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");
    if (sdf3d::SdfGraphNode* node = graph.node(firstTranslate)) {
        node->payload.parameters["x"] = -2.0f;
    }
    if (sdf3d::SdfGraphNode* node = graph.node(secondTranslate)) {
        node->payload.parameters["x"] = 4.0f;
    }
    expect(graph.link(sphere, "sdf", firstTranslate, "child"), testName, "Expected sphere to first translate link.", failures);
    expect(graph.link(firstTranslate, "sdf", unionNode, "left"), testName, "Expected first translate to union left link.", failures);
    expect(graph.link(firstTranslate, "sdf", secondTranslate, "child"), testName, "Expected first translate to second translate link.", failures);
    expect(graph.link(secondTranslate, "sdf", unionNode, "right"), testName, "Expected second translate to union right link.", failures);
    expect(graph.link(unionNode, "sdf", graph.outputNode(), "surface"), testName, "Expected union to output link.", failures);

    const sdf3d::SdfGraphNodeId pickedLeft = sdf3d::GraphSystem::pickNodeByRay(
        graph,
        {-2.0f, 0.0f, 5.0f},
        glm::normalize(glm::vec3{0.0f, 0.0f, -1.0f}));
    expect(pickedLeft == firstTranslate, testName, "Expected left boolean input to select first transform.", failures);

    const sdf3d::SdfGraphNodeId pickedRight = sdf3d::GraphSystem::pickNodeByRay(
        graph,
        {2.0f, 0.0f, 5.0f},
        glm::normalize(glm::vec3{0.0f, 0.0f, -1.0f}));
    expect(pickedRight == secondTranslate, testName, "Expected right boolean input to select second transform.", failures);
}

void testPickNodeByRaySelectsFirstBooleanInputWhenBranchesOverlap(std::vector<TestFailure>& failures)
{
    const std::string testName = "pick node by ray selects first boolean input when branches overlap";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId firstTranslate = graph.createNode(sdf3d::SdfNodeType::Translate, "Trans1");
    const sdf3d::SdfGraphNodeId secondTranslate = graph.createNode(sdf3d::SdfNodeType::Translate, "Trans2");
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");
    expect(graph.link(sphere, "sdf", firstTranslate, "child"), testName, "Expected sphere to first translate link.", failures);
    expect(graph.link(firstTranslate, "sdf", unionNode, "left"), testName, "Expected first translate to union left link.", failures);
    expect(graph.link(firstTranslate, "sdf", secondTranslate, "child"), testName, "Expected first translate to second translate link.", failures);
    expect(graph.link(secondTranslate, "sdf", unionNode, "right"), testName, "Expected second translate to union right link.", failures);
    expect(graph.link(unionNode, "sdf", graph.outputNode(), "surface"), testName, "Expected union to output link.", failures);

    const sdf3d::SdfGraphNodeId picked = sdf3d::GraphSystem::pickNodeByRay(
        graph,
        {0.0f, 0.0f, 5.0f},
        glm::normalize(glm::vec3{0.0f, 0.0f, -1.0f}));
    expect(picked == firstTranslate, testName, "Expected overlapping left branch to remain selectable.", failures);
}

void testPickNodeByRaySelectsNestedBooleanInputTransform(std::vector<TestFailure>& failures)
{
    const std::string testName = "pick node by ray selects nested boolean input transform";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere1 = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere1");
    const sdf3d::SdfGraphNodeId sphere2 = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere2");
    const sdf3d::SdfGraphNodeId sphere3 = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere3");
    const sdf3d::SdfGraphNodeId trans1 = graph.createNode(sdf3d::SdfNodeType::Translate, "T1");
    const sdf3d::SdfGraphNodeId trans2 = graph.createNode(sdf3d::SdfNodeType::Translate, "T2");
    const sdf3d::SdfGraphNodeId trans3 = graph.createNode(sdf3d::SdfNodeType::Translate, "T3");
    const sdf3d::SdfGraphNodeId bool1 = graph.createNode(sdf3d::SdfNodeType::Union, "Bool1");
    const sdf3d::SdfGraphNodeId bool2 = graph.createNode(sdf3d::SdfNodeType::Union, "Bool2");
    if (sdf3d::SdfGraphNode* node = graph.node(trans1)) {
        node->payload.parameters["x"] = -4.0f;
    }
    if (sdf3d::SdfGraphNode* node = graph.node(trans2)) {
        node->payload.parameters["x"] = 0.0f;
    }
    if (sdf3d::SdfGraphNode* node = graph.node(trans3)) {
        node->payload.parameters["x"] = 4.0f;
    }
    expect(graph.link(sphere1, "sdf", trans1, "child"), testName, "Expected sphere1 to T1 link.", failures);
    expect(graph.link(sphere2, "sdf", trans2, "child"), testName, "Expected sphere2 to T2 link.", failures);
    expect(graph.link(sphere3, "sdf", trans3, "child"), testName, "Expected sphere3 to T3 link.", failures);
    expect(graph.link(trans1, "sdf", bool1, "left"), testName, "Expected T1 to Bool1 left link.", failures);
    expect(graph.link(trans2, "sdf", bool1, "right"), testName, "Expected T2 to Bool1 right link.", failures);
    expect(graph.link(bool1, "sdf", bool2, "left"), testName, "Expected Bool1 to Bool2 left link.", failures);
    expect(graph.link(trans3, "sdf", bool2, "right"), testName, "Expected T3 to Bool2 right link.", failures);
    expect(graph.link(bool2, "sdf", graph.outputNode(), "surface"), testName, "Expected Bool2 to output link.", failures);

    const sdf3d::SdfGraphNodeId pickedT1 = sdf3d::GraphSystem::pickNodeByRay(
        graph,
        {-4.0f, 0.0f, 5.0f},
        glm::normalize(glm::vec3{0.0f, 0.0f, -1.0f}));
    expect(pickedT1 == trans1, testName, "Expected nested Bool1.left to select T1, not Bool1.", failures);

    const sdf3d::SdfGraphNodeId pickedT2 = sdf3d::GraphSystem::pickNodeByRay(
        graph,
        {0.0f, 0.0f, 5.0f},
        glm::normalize(glm::vec3{0.0f, 0.0f, -1.0f}));
    expect(pickedT2 == trans2, testName, "Expected nested Bool1.right to select T2, not Bool1.", failures);

    const sdf3d::SdfGraphNodeId pickedT3 = sdf3d::GraphSystem::pickNodeByRay(
        graph,
        {4.0f, 0.0f, 5.0f},
        glm::normalize(glm::vec3{0.0f, 0.0f, -1.0f}));
    expect(pickedT3 == trans3, testName, "Expected Bool2.right to select T3.", failures);
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
    expect(graph.link(firstTranslate, "sdf", unionNode, "left"), testName, "Expected first translate to union left link.", failures);
    expect(graph.link(firstTranslate, "sdf", secondTranslate, "child"), testName, "Expected first translate to second translate link.", failures);
    expect(graph.link(secondTranslate, "sdf", unionNode, "right"), testName, "Expected second translate to union right link.", failures);
    expect(graph.link(unionNode, "sdf", graph.outputNode(), "surface"), testName, "Expected union to output link.", failures);

    graph.setSelectedNode(firstTranslate);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == firstTranslate, testName, "Expected selected transform to highlight itself.", failures);

    graph.setSelectedNode(sphere);
    expect(sdf3d::GraphSystem::highlightNodeForSelection(graph) == firstTranslate, testName, "Expected primitive selection to stop at branched transform.", failures);
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

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testDuplicateSelectionCopiesInternalLinksOnly(failures);
    testDuplicateSelectionSkipsOutput(failures);
    testEffectiveValidityDropsInvalidUpstream(failures);
    testLoweredRequiredInputRules(failures);
    testMaterialRegistryRenameAndSafeDelete(failures);
    testMaterialSourceDeleteCleansUnreferencedRegistryEntry(failures);
    testMaterialSourceDeleteKeepsRegistryEntryReferencedByOverride(failures);
    testCollectNodeParamsPacksTransformValues(failures);
    testCollectNodeParamsNormalizesRotateQuaternion(failures);
    testPickNodeByRaySelectsTranslatedPrimitive(failures);
    testPickNodeByRayMissesEmptySpace(failures);
    testPickNodeByRaySelectsBooleanInputBranchTransform(failures);
    testPickNodeByRaySelectsFirstBooleanInputWhenBranchesOverlap(failures);
    testPickNodeByRaySelectsNestedBooleanInputTransform(failures);
    testHighlightNodeForSelectionUsesTransformWrapper(failures);
    testHighlightNodeForSelectionFollowsTransformChain(failures);
    testHighlightNodeForSelectionStopsAtBranchedTransform(failures);
    testEnsureTransformWrapperCanChainFromTransform(failures);
    testEnsureTransformWrapperUsesCanonicalOrder(failures);
    testEnsureTransformWrapperReusesExistingChainParent(failures);
    testEnsureTransformWrapperReusesExistingChainChild(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All GraphSystem tests passed.\n";
    return 0;
}
