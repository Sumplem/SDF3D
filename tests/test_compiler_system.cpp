#include "sdf3d/systems/CompilerSystem.h"

#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/scene/SdfGraphCompiler.h"
#include "sdf3d/systems/GlslEmitter.h"

#include <iostream>
#include <string>
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
    expect(contains(compiled.glsl, "return " + block.rootFunctionName + "(p);"), testName, "Expected sceneSDF to call root helper.", failures);
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
    expect(contains(compiled.glsl, "sdf_node_" + std::to_string(firstSphere) + "(p) < sdf_node_" + std::to_string(secondSphere) + "(p)"), testName, "Expected union pick to compare child distances.", failures);
    expect(contains(compiled.glsl, "? " + std::to_string(firstSphere) + " : " + std::to_string(secondSphere)), testName, "Expected pick-id branch ids.", failures);
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
    expect(contains(compiled.glsl, "return " + std::to_string(group) + ";"), testName, "Expected group instance id returned for picking.", failures);
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
    expect(contains(compiled.glsl, "return " + std::to_string(scale) + ";"), testName, "Expected transform wrapper id returned for picking.", failures);
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

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testCompileSphere(failures);
    testCompileMaterialOverride(failures);
    testCompileEmpty(failures);
    testGraphLoweringPreservesStableIdsForHelpers(failures);
    testScenePickIdSelectsNearestUnionBranch(failures);
    testScenePickIdReturnsGroupInstanceId(failures);
    testScenePickIdReturnsTransformWrapperId(failures);
    testSceneNodeContainsNestedTransform(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All CompilerSystem tests passed.\n";
    return 0;
}
