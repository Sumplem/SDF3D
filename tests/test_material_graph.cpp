#include "sdf3d/scene/MaterialGraph.h"
#include "sdf3d/systems/MaterialGraphCompiler.h"

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

bool contains(const std::string& text, const std::string& expected)
{
    return text.find(expected) != std::string::npos;
}

void testCreateLinkDeleteSelect(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph create link delete select";
    sdf3d::MaterialGraph graph;
    const sdf3d::MaterialGraphNodeId color = graph.createNode(sdf3d::MaterialGraphNodeType::ColorConstant, "Color");
    const sdf3d::MaterialGraphNodeId pbr = graph.createNode(sdf3d::MaterialGraphNodeType::PbrMaterial, "PBR");

    expect(graph.link(color, "color", pbr, "albedo"), testName, "Expected color link to PBR albedo.", failures);
    expect(!graph.link(color, "color", pbr, "roughness"), testName, "Expected incompatible socket link rejected.", failures);
    expect(graph.setSelectedNode(color), testName, "Expected selectable material node.", failures);
    expect(graph.selectedNode() == color, testName, "Expected selected node stored.", failures);
    expect(graph.deleteNode(color), testName, "Expected node delete.", failures);
    expect(graph.links().empty(), testName, "Expected connected links removed.", failures);
}

void testRejectCycle(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph rejects cycles";
    sdf3d::MaterialGraph graph;
    const sdf3d::MaterialGraphNodeId a = graph.createNode(sdf3d::MaterialGraphNodeType::MixColor, "A");
    const sdf3d::MaterialGraphNodeId b = graph.createNode(sdf3d::MaterialGraphNodeType::MixColor, "B");

    expect(graph.link(a, "color", b, "a"), testName, "Expected first color link.", failures);
    expect(!graph.link(b, "color", a, "a"), testName, "Expected cycle link rejected.", failures);
}

void testReplaceDataPreservesStableIds(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph replace data preserves ids";
    std::vector<sdf3d::MaterialGraphNode> nodes;
    nodes.push_back({1, sdf3d::MaterialGraphNodeType::MaterialOutput, "Out"});
    nodes.push_back({7, sdf3d::MaterialGraphNodeType::ColorConstant, "Color"});

    sdf3d::MaterialGraph graph;
    expect(graph.replaceData(8, 1, nodes, {}), testName, "Expected replace data success.", failures);
    expect(graph.outputNode() == 1, testName, "Expected output ID preserved.", failures);
    expect(graph.node(7) != nullptr, testName, "Expected node ID preserved.", failures);
    expect(graph.nextNodeIdForSerialization() == 8, testName, "Expected next ID preserved.", failures);
}

void testCompilerEmitsPbrGraph(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph compiler emits pbr graph";
    sdf3d::SdfMaterial material;
    material.albedo = {0.2f, 0.3f, 0.4f};
    material.roughness = 0.6f;
    sdf3d::MaterialDefinition definition{3, "Paint", material, sdf3d::makeMaterialGraphFromMaterial(material)};
    sdf3d::SdfCompileResult result;
    const std::string glsl = sdf3d::MaterialGraphCompiler{}.emitMaterialFunction(definition, result);

    expect(contains(glsl, "SdfMaterialSample sdf3d_material_3(vec3 p)"), testName, "Expected material helper name.", failures);
    expect(contains(glsl, "SdfMaterialSample("), testName, "Expected material sample constructor.", failures);
    expect(result.materialFunctionByRegistryId[3] == "sdf3d_material_3", testName, "Expected function map entry.", failures);
}

void testCompilerUsesProceduralPoint(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph compiler uses procedural point";
    sdf3d::SdfMaterial material;
    material.type = sdf3d::SdfMaterialType::ValueNoise;
    sdf3d::MaterialDefinition definition{4, "Noise", material, sdf3d::makeMaterialGraphFromMaterial(material)};
    sdf3d::SdfCompileResult result;
    const std::string glsl = sdf3d::MaterialGraphCompiler{}.emitMaterialFunction(definition, result);

    expect(contains(glsl, "sdf3d_valueNoise3d(p * max("), testName, "Expected procedural material to sample from p.", failures);
}

void testCompilerUsesEmbeddedDefaultsAndLinkOverrides(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph compiler embedded defaults and overrides";
    sdf3d::SdfMaterial material;
    material.albedo = {0.9f, 0.9f, 0.9f};
    material.roughness = 0.9f;

    sdf3d::MaterialGraph graph;
    const sdf3d::MaterialGraphNodeId pbr = graph.createNode(sdf3d::MaterialGraphNodeType::PbrMaterial, "PBR");
    const sdf3d::MaterialGraphNodeId roughness = graph.createNode(sdf3d::MaterialGraphNodeType::FloatConstant, "Override Roughness");
    if (sdf3d::MaterialGraphNode* node = graph.node(pbr)) {
        node->color = {0.11f, 0.22f, 0.33f};
        node->roughness = 0.42f;
    }
    if (sdf3d::MaterialGraphNode* node = graph.node(roughness)) {
        node->value = 0.77f;
    }
    expect(graph.link(roughness, "value", pbr, "roughness"), testName, "Expected roughness override link.", failures);
    expect(graph.link(pbr, "material", graph.outputNode(), "material"), testName, "Expected PBR output link.", failures);

    sdf3d::MaterialDefinition definition{5, "Embedded", material, graph};
    sdf3d::SdfCompileResult result;
    const std::string glsl = sdf3d::MaterialGraphCompiler{}.emitMaterialFunction(definition, result);

    expect(contains(glsl, "vec3(0.110000, 0.220000, 0.330000)"), testName, "Expected embedded albedo default.", failures);
    expect(contains(glsl, "0.770000"), testName, "Expected linked roughness override.", failures);
    expect(!contains(glsl, "0.420000"), testName, "Expected linked roughness to override embedded default.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;
    testCreateLinkDeleteSelect(failures);
    testRejectCycle(failures);
    testReplaceDataPreservesStableIds(failures);
    testCompilerEmitsPbrGraph(failures);
    testCompilerUsesProceduralPoint(failures);
    testCompilerUsesEmbeddedDefaultsAndLinkOverrides(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All MaterialGraph tests passed.\n";
    return 0;
}
