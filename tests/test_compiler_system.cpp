#include "sdf3d/systems/CompilerSystem.h"

#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/scene/SdfGraphCompiler.h"
#include "sdf3d/systems/GlslEmitter.h"
#include "../src/systems/glsl_emitter/GlslEmitterMath.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct TestFailure {
    std::string name;
    std::string message;
};

bool contains(const std::string& text, const std::string& expected)
{
    return text.find(expected) != std::string::npos;
}

std::size_t countOccurrences(const std::string& text, const std::string& expected)
{
    std::size_t count = 0;
    std::size_t offset = 0;
    while ((offset = text.find(expected, offset)) != std::string::npos) {
        ++count;
        offset += expected.size();
    }
    return count;
}

std::string sceneMaterialBody(const std::string& glsl)
{
    const std::string marker = "SdfMaterialSample sceneMaterial(vec3 p)\n{\n";
    const std::size_t bodyStart = glsl.find(marker);
    if (bodyStart == std::string::npos) {
        return "";
    }

    const std::size_t contentStart = bodyStart + marker.size();
    const std::size_t bodyEnd = glsl.find("\n}", contentStart);
    if (bodyEnd == std::string::npos) {
        return "";
    }
    return glsl.substr(contentStart, bodyEnd - contentStart);
}

void expect(bool condition, const std::string& testName, const std::string& message, std::vector<TestFailure>& failures)
{
    if (!condition) {
        failures.push_back({testName, message});
    }
}

void testCompileSphere(std::vector<TestFailure>& failures)
{
    const std::string testName = "compile sphere";
    const sdf3d::CompilerSystem compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(sdf3d::makeSphereNode());

    expect(result.errors.empty(), testName, "Expected no compile errors.", failures);
    expect(contains(result.glsl, "SdfMaterialSample sceneMaterial(vec3 p)"), testName, "Expected deferred material entry point.", failures);
    expect(!contains(result.glsl, "sceneSDFWithMaterial"), testName, "Expected legacy material entry point removed.", failures);
    expect(result.materials.size() == 1, testName, "Expected default material output.", failures);
}

void testCompileMaterialOverride(std::vector<TestFailure>& failures)
{
    const std::string testName = "compile material override";
    const sdf3d::CompilerSystem compiler;
    sdf3d::SdfNodePtr material = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Red");
    material->material.albedo = {1.0f, 0.0f, 0.0f};
    material->children.push_back(sdf3d::makeSphereNode());
    const sdf3d::SdfCompileResult result = compiler.compile(material);

    expect(result.errors.empty(), testName, "Expected no compile errors.", failures);
    expect(contains(result.glsl, "SdfMaterialSample sceneMaterial(vec3 p)"), testName, "Expected deferred material entry point.", failures);
    expect(contains(result.glsl, "return sampleMaterial(1, p);"), testName, "Expected sceneMaterial to return override sample.", failures);
    expect(result.materials.size() == 2, testName, "Expected default plus override material.", failures);
    if (result.materials.size() == 2) {
        expect(result.materials[1].material.albedo.x == 1.0f, testName, "Expected override material preserved.", failures);
    }
}

void testCompileEmpty(std::vector<TestFailure>& failures)
{
    const std::string testName = "compile empty";
    const sdf3d::CompilerSystem compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(nullptr);

    expect(!result.errors.empty(), testName, "Expected empty compile error.", failures);
    expect(contains(result.glsl, "return 1e6;"), testName, "Expected no-hit fallback.", failures);
}

void testGraphLoweringPreservesStableIdsForHelpers(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph stable ids for helpers";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    graph.link(sphere, "sdf", material, "sdf");
    graph.link(material, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfGraphLowerResult lowered = sdf3d::lowerSdfGraphToTree(graph);
    sdf3d::SdfCompileResult result;
    const sdf3d::GlslSdfHelperBlock block = sdf3d::GlslEmitter{}.emitSdfHelpers(lowered.root, result);

    expect(lowered.errors.empty(), testName, "Expected no lower errors.", failures);
    expect(result.errors.empty(), testName, "Expected no helper errors.", failures);
    expect(block.rootFunctionName == ("sdf_node_" + std::to_string(material)), testName, "Expected root helper to use graph node ID.", failures);
    expect(block.helpers.size() == 2, testName, "Expected sphere and material helpers.", failures);
    if (block.helpers.size() == 2) {
        expect(block.helpers[0].functionName == ("sdf_node_" + std::to_string(sphere)), testName, "Expected sphere helper to use graph node ID.", failures);
        expect(block.helpers[1].functionName == ("sdf_node_" + std::to_string(material)), testName, "Expected material helper to use graph node ID.", failures);
    }

    const sdf3d::SdfCompileResult compiled = sdf3d::CompilerSystem{}.compile(graph);
    expect(compiled.errors.empty(), testName, "Expected graph compile errors to stay empty.", failures);
    expect(contains(compiled.glsl, "float " + block.rootFunctionName + "(vec3 p)"), testName, "Expected final GLSL to include root helper.", failures);
    expect(contains(compiled.glsl, "vec2 sceneSDFWithId(vec3 p)"), testName, "Expected combined distance/id entry point.", failures);
    expect(contains(compiled.glsl, "return sceneSDFWithId(p).x;"), testName, "Expected sceneSDF to reuse combined distance/id entry point.", failures);
    expect(contains(compiled.glsl, "SdfMaterialSample sceneMaterial(vec3 p)"), testName, "Expected deferred material entry point.", failures);
    expect(!contains(compiled.glsl, "sceneSDFWithMaterial"), testName, "Expected legacy material entry point removed.", failures);
}

void testScenePickIdSelectsNearestUnionBranch(std::vector<TestFailure>& failures)
{
    const std::string testName = "scenePickId selects nearest union branch";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId firstSphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "First Sphere");
    const sdf3d::SdfGraphNodeId secondSphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Second Sphere");
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");
    expect(graph.link(firstSphere, "sdf", unionNode, "inputs"), testName, "Expected first sphere linked.", failures);
    expect(graph.link(secondSphere, "sdf", unionNode, "inputs"), testName, "Expected second sphere linked.", failures);
    expect(graph.link(unionNode, "sdf", graph.outputNode(), "surface"), testName, "Expected union output linked.", failures);

    const sdf3d::SdfCompileResult compiled = sdf3d::CompilerSystem{}.compile(graph);
    expect(compiled.errors.empty(), testName, "Expected graph compile errors to stay empty.", failures);
    expect(contains(compiled.glsl, "int scenePickId(vec3 p)"), testName, "Expected pick-id entry point.", failures);
    expect(contains(compiled.glsl, "vec2 sceneSDFWithId(vec3 p)"), testName, "Expected combined distance/id entry point.", failures);
    expect(contains(compiled.glsl, "sdf_node_" + std::to_string(firstSphere) + "(p)"), testName, "Expected first branch distance in combined pick.", failures);
    expect(contains(compiled.glsl, "sdf_node_" + std::to_string(secondSphere) + "(p)"), testName, "Expected second branch distance in combined pick.", failures);
    expect(contains(compiled.glsl, "? sdf3d_hit"), testName, "Expected pick-id branch selection.", failures);
}

void testSceneSdfWithIdSmoothUnionUsesBlendWinner(std::vector<TestFailure>& failures)
{
    const std::string testName = "sceneSDFWithId smooth union uses blend winner";
    sdf3d::SdfNodePtr first = sdf3d::makeSphereNode();
    first->stableId = 10;
    sdf3d::SdfNodePtr second = sdf3d::makeSphereNode();
    second->stableId = 20;
    sdf3d::SdfNodePtr smoothUnion = sdf3d::makeSmoothUnionNode({first, second}, 0.5f);
    smoothUnion->stableId = 30;

    const sdf3d::SdfCompileResult compiled = sdf3d::CompilerSystem{}.compile(smoothUnion);

    expect(compiled.errors.empty(), testName, "Expected compile errors to stay empty.", failures);
    expect(contains(compiled.glsl, "vec2 sceneSDFWithId(vec3 p)"), testName, "Expected combined distance/id entry point.", failures);
    expect(contains(compiled.glsl, "clamp(0.5 + 0.5 * (sdf3d_hit"), testName, "Expected smooth boundary blend weight.", failures);
    expect(contains(compiled.glsl, "> 0.5 ? sdf3d_hit"), testName, "Expected smooth winner id from blend weight.", failures);
}

void testScenePickIdReturnsGroupInstanceId(std::vector<TestFailure>& failures)
{
    const std::string testName = "scenePickId returns group instance id";
    sdf3d::SdfGraph groupGraph;
    const sdf3d::SdfGraphNodeId sphere = groupGraph.createNode(sdf3d::SdfNodeType::Sphere, "Group Sphere");
    expect(groupGraph.link(sphere, "sdf", groupGraph.outputNode(), "surface"), testName, "Expected group sphere linked.", failures);

    sdf3d::GraphGroupRegistry groups;
    const sdf3d::GroupDefId definitionId = groups.createDefinition("Group", groupGraph);
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId group = graph.createNode(sdf3d::SdfNodeType::Group, "Group");
    if (sdf3d::SdfGraphNode* node = graph.node(group)) {
        node->payload.groupDefinitionId = definitionId;
    }
    expect(graph.link(group, "sdf", graph.outputNode(), "surface"), testName, "Expected group output linked.", failures);

    const sdf3d::SdfCompileResult compiled = sdf3d::CompilerSystem{}.compile(graph, groups);
    expect(compiled.errors.empty(), testName, "Expected group compile errors to stay empty.", failures);
    expect(contains(compiled.glsl, "int scenePickId(vec3 p)"), testName, "Expected pick-id entry point.", failures);
    expect(contains(compiled.glsl, "vec2(sdf_node_" + std::to_string(group) + "(p), " + std::to_string(group) + ".0)"), testName, "Expected group instance id returned for picking.", failures);
}

void testScenePickIdReturnsTransformWrapperId(std::vector<TestFailure>& failures)
{
    const std::string testName = "scenePickId returns transform wrapper id";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId scale = graph.createNode(sdf3d::SdfNodeType::Scale, "Scale");
    expect(graph.link(sphere, "sdf", scale, "child"), testName, "Expected sphere linked into scale.", failures);
    expect(graph.link(scale, "sdf", graph.outputNode(), "surface"), testName, "Expected scale output linked.", failures);

    const sdf3d::SdfCompileResult compiled = sdf3d::CompilerSystem{}.compile(graph);
    expect(compiled.errors.empty(), testName, "Expected graph compile errors to stay empty.", failures);
    expect(contains(compiled.glsl, "vec2(sdf_node_" + std::to_string(scale) + "(p), " + std::to_string(scale) + ".0)"), testName, "Expected transform wrapper id returned for picking.", failures);
}

void testSceneNodeContainsNestedTransform(std::vector<TestFailure>& failures)
{
    const std::string testName = "sceneNodeContains nested transform";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId rotate = graph.createNode(sdf3d::SdfNodeType::Rotate, "Rotate");
    const sdf3d::SdfGraphNodeId scale = graph.createNode(sdf3d::SdfNodeType::Scale, "Scale");
    expect(graph.link(sphere, "sdf", rotate, "child"), testName, "Expected sphere linked into rotate.", failures);
    expect(graph.link(rotate, "sdf", scale, "child"), testName, "Expected rotate linked into scale.", failures);
    expect(graph.link(scale, "sdf", graph.outputNode(), "surface"), testName, "Expected scale output linked.", failures);

    const sdf3d::SdfCompileResult compiled = sdf3d::CompilerSystem{}.compile(graph);
    expect(compiled.errors.empty(), testName, "Expected graph compile errors to stay empty.", failures);
    expect(contains(compiled.glsl, "bool sceneNodeContains(int nodeId, int visibleNodeId)"), testName, "Expected containment entry point.", failures);
    expect(contains(compiled.glsl, "case " + std::to_string(scale) + ": return"), testName, "Expected scale containment case.", failures);
    expect(contains(compiled.glsl, "nodeId == " + std::to_string(sphere)), testName, "Expected containment cases to include sphere.", failures);
    expect(contains(compiled.glsl, "nodeId == " + std::to_string(rotate)), testName, "Expected scale containment case to include rotate.", failures);
}

void testSceneMaterialCachesBooleanChildDistances(std::vector<TestFailure>& failures)
{
    const std::string testName = "sceneMaterial caches boolean child distances";
    sdf3d::SdfNodePtr first = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "First");
    first->stableId = 30;
    first->children.push_back(sdf3d::makeSphereNode("First Sphere"));
    sdf3d::SdfNodePtr second = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Second");
    second->stableId = 40;
    second->children.push_back(sdf3d::makeBoxNode({1.0f, 1.0f, 1.0f}, "Second Box"));
    sdf3d::SdfNodePtr third = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Third");
    third->stableId = 50;
    third->children.push_back(sdf3d::makeSphereNode("Third Sphere"));
    sdf3d::SdfNodePtr unionNode = sdf3d::makeUnionNode({first, second, third});
    unionNode->stableId = 60;

    const sdf3d::SdfCompileResult compiled = sdf3d::CompilerSystem{}.compile(unionNode);
    const std::string body = sceneMaterialBody(compiled.glsl);

    expect(compiled.errors.empty(), testName, "Expected compile errors to stay empty.", failures);
    expect(!body.empty(), testName, "Expected sceneMaterial body.", failures);
    expect(contains(body, "float sdf3d_distance"), testName, "Expected local distance temps.", failures);
    expect(countOccurrences(body, "sdf_node_30(p)") == 1, testName, "Expected first child distance helper once.", failures);
    expect(countOccurrences(body, "sdf_node_40(p)") == 1, testName, "Expected second child distance helper once.", failures);
    expect(countOccurrences(body, "sdf_node_50(p)") == 1, testName, "Expected third child distance helper once.", failures);
}

void testSceneMaterialUsesSharedDomainWarp(std::vector<TestFailure>& failures)
{
    const std::string testName = "sceneMaterial uses shared domain warp";
    sdf3d::SdfNodePtr sphere = sdf3d::makeSphereNode();
    sphere->stableId = 10;
    sdf3d::SdfNodePtr material = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    material->stableId = 30;
    material->children.push_back(sphere);
    sdf3d::SdfNodePtr twist = sdf3d::makeSdfNode(sdf3d::SdfNodeType::Twist, "Twist");
    twist->stableId = 20;
    twist->children.push_back(material);

    const sdf3d::SdfCompileResult compiled = sdf3d::CompilerSystem{}.compile(twist);
    const std::string body = sceneMaterialBody(compiled.glsl);
    const std::unordered_map<uint64_t, uint32_t> nodeParamSlots{{twist->stableId, 1u}};
    const sdf3d::glsl_emitter::DomainWarpExpr warp = sdf3d::glsl_emitter::domainWarpFor(*twist, twist->stableId, sdf3d::GlslEmitMode::Runtime, "p", nodeParamSlots);

    expect(compiled.errors.empty(), testName, "Expected compile errors to stay empty.", failures);
    expect(contains(compiled.glsl, "sdf_node_30(" + warp.point + ")"), testName, "Expected SDF helper to use shared warp point.", failures);
    expect(contains(body, "sampleMaterial(1, " + warp.point + ")"), testName, "Expected material evaluation to use shared warp point.", failures);
}

void testSceneMaterialDoesNotDoubleEvaluateBooleanBranchDistance(std::vector<TestFailure>& failures)
{
    const std::string testName = "sceneMaterial does not double evaluate boolean branch distance";
    sdf3d::SdfNodePtr leftSphere = sdf3d::makeSphereNode();
    leftSphere->stableId = 10;
    sdf3d::SdfNodePtr leftMaterial = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Left Material");
    leftMaterial->stableId = 20;
    leftMaterial->children.push_back(leftSphere);

    sdf3d::SdfNodePtr rightSphere = sdf3d::makeSphereNode();
    rightSphere->stableId = 30;
    sdf3d::SdfNodePtr rightMaterial = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Right Material");
    rightMaterial->stableId = 40;
    rightMaterial->children.push_back(rightSphere);

    sdf3d::SdfNodePtr unionNode = sdf3d::makeUnionNode({leftMaterial, rightMaterial});
    unionNode->stableId = 50;

    const sdf3d::SdfCompileResult compiled = sdf3d::CompilerSystem{}.compile(unionNode);
    const std::string body = sceneMaterialBody(compiled.glsl);

    expect(compiled.errors.empty(), testName, "Expected compile errors to stay empty.", failures);
    expect(countOccurrences(body, "sdf_node_20(p)") == 1, testName, "Expected left branch distance evaluated once in sceneMaterial.", failures);
    expect(countOccurrences(body, "sdf_node_40(p)") == 1, testName, "Expected right branch distance evaluated once in sceneMaterial.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testCompileSphere(failures);
    testCompileMaterialOverride(failures);
    testCompileEmpty(failures);
    testGraphLoweringPreservesStableIdsForHelpers(failures);
    testScenePickIdSelectsNearestUnionBranch(failures);
    testSceneSdfWithIdSmoothUnionUsesBlendWinner(failures);
    testScenePickIdReturnsGroupInstanceId(failures);
    testScenePickIdReturnsTransformWrapperId(failures);
    testSceneNodeContainsNestedTransform(failures);
    testSceneMaterialCachesBooleanChildDistances(failures);
    testSceneMaterialUsesSharedDomainWarp(failures);
    testSceneMaterialDoesNotDoubleEvaluateBooleanBranchDistance(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All CompilerSystem tests passed.\n";
    return 0;
}
