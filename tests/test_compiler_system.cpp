#include "sdf3d/systems/CompilerSystem.h"

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
    expect(contains(result.glsl, "return sampleMaterial(1);"), testName, "Expected sceneMaterial to return override sample.", failures);
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

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testCompileSphere(failures);
    testCompileMaterialOverride(failures);
    testCompileEmpty(failures);
    testGraphLoweringPreservesStableIdsForHelpers(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All CompilerSystem tests passed.\n";
    return 0;
}
