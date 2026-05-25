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

int countNodesOfType(const sdf3d::MaterialGraph& graph, sdf3d::MaterialGraphNodeType type)
{
    int count = 0;
    for (const sdf3d::MaterialGraphNode& node : graph.nodes()) {
        if (node.type == type) {
            ++count;
        }
    }
    return count;
}

bool hasLink(
    const sdf3d::MaterialGraph& graph,
    sdf3d::MaterialGraphNodeId fromNode,
    const std::string& fromSocket,
    sdf3d::MaterialGraphNodeId toNode,
    const std::string& toSocket)
{
    for (const sdf3d::MaterialGraphLink& link : graph.links()) {
        if (link.fromNode == fromNode
            && link.fromSocket == fromSocket
            && link.toNode == toNode
            && link.toSocket == toSocket) {
            return true;
        }
    }
    return false;
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

void testLinkCompatibilityUsesSocketType(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph link compatibility uses socket type";
    sdf3d::MaterialGraph graph;
    const sdf3d::MaterialGraphNodeId color = graph.createNode(sdf3d::MaterialGraphNodeType::ColorConstant, "Color");
    const sdf3d::MaterialGraphNodeId mix = graph.createNode(sdf3d::MaterialGraphNodeType::MixColor, "Mix");
    const sdf3d::MaterialGraphNodeId pbr = graph.createNode(sdf3d::MaterialGraphNodeType::PbrMaterial, "PBR");

    expect(graph.link(color, "color", pbr, "albedo"), testName, "Expected Color output to link to Color input.", failures);
    expect(graph.link(mix, "color", pbr, "albedo"), testName, "Expected Mix Color output to link to Color input.", failures);
    expect(!graph.link(color, "color", pbr, "roughness"), testName, "Expected Color output to reject Float input.", failures);
    expect(!graph.link(color, "missing", pbr, "albedo"), testName, "Expected missing output socket rejected.", failures);
    expect(!graph.link(color, "color", pbr, "missing"), testName, "Expected missing input socket rejected.", failures);
}

void testValueNoiseIsReusableFloat(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph value noise is reusable float";
    sdf3d::MaterialGraph graph;
    const sdf3d::MaterialGraphNodeId noise = graph.createNode(sdf3d::MaterialGraphNodeType::ValueNoise, "Noise");
    const sdf3d::MaterialGraphNodeId mix = graph.createNode(sdf3d::MaterialGraphNodeType::MixColor, "Mix");
    const sdf3d::MaterialGraphNodeId pbr = graph.createNode(sdf3d::MaterialGraphNodeType::PbrMaterial, "PBR");

    expect(graph.link(noise, "factor", mix, "factor"), testName, "Expected ValueNoise factor to link to MixColor factor.", failures);
    expect(graph.link(noise, "factor", pbr, "roughness"), testName, "Expected ValueNoise factor to link to PBR roughness.", failures);
    expect(!graph.link(noise, "factor", pbr, "albedo"), testName, "Expected ValueNoise factor to reject Color input.", failures);
}

void testCheckerOutputsColorAndFactor(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph checker outputs color and factor";
    sdf3d::MaterialGraph graph;
    const sdf3d::MaterialGraphNodeId checker = graph.createNode(sdf3d::MaterialGraphNodeType::CheckerPattern, "Checker");
    const sdf3d::MaterialGraphNodeId mix = graph.createNode(sdf3d::MaterialGraphNodeType::MixColor, "Mix");
    const sdf3d::MaterialGraphNodeId pbr = graph.createNode(sdf3d::MaterialGraphNodeType::PbrMaterial, "PBR");

    expect(graph.link(checker, "factor", mix, "factor"), testName, "Expected Checker factor to drive MixColor factor.", failures);
    expect(graph.link(checker, "color", pbr, "albedo"), testName, "Expected Checker color to drive PBR albedo.", failures);
    expect(graph.link(checker, "factor", pbr, "roughness"), testName, "Expected Checker factor to drive PBR roughness.", failures);
    expect(!graph.link(checker, "color", pbr, "roughness"), testName, "Expected Checker color to reject Float input.", failures);
    expect(!graph.link(checker, "factor", pbr, "albedo"), testName, "Expected Checker factor to reject Color input.", failures);
}

void testCoreProceduralNodesLinkByType(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph core procedural nodes link by type";
    sdf3d::MaterialGraph graph;
    const sdf3d::MaterialGraphNodeId noise = graph.createNode(sdf3d::MaterialGraphNodeType::ValueNoise, "Noise");
    const sdf3d::MaterialGraphNodeId ramp = graph.createNode(sdf3d::MaterialGraphNodeType::ColorRamp, "Ramp");
    const sdf3d::MaterialGraphNodeId add = graph.createNode(sdf3d::MaterialGraphNodeType::AddColor, "Add");
    const sdf3d::MaterialGraphNodeId subtract = graph.createNode(sdf3d::MaterialGraphNodeType::SubtractColor, "Subtract");
    const sdf3d::MaterialGraphNodeId power = graph.createNode(sdf3d::MaterialGraphNodeType::PowerFloat, "Power");
    const sdf3d::MaterialGraphNodeId clamp = graph.createNode(sdf3d::MaterialGraphNodeType::ClampFloat, "Clamp");
    const sdf3d::MaterialGraphNodeId pbr = graph.createNode(sdf3d::MaterialGraphNodeType::PbrMaterial, "PBR");

    expect(graph.link(noise, "factor", ramp, "factor"), testName, "Expected noise factor to drive ColorRamp factor.", failures);
    expect(graph.link(ramp, "color", pbr, "albedo"), testName, "Expected ColorRamp color to drive albedo.", failures);
    expect(graph.link(add, "color", pbr, "albedo"), testName, "Expected AddColor output to drive albedo.", failures);
    expect(graph.link(subtract, "color", pbr, "albedo"), testName, "Expected SubtractColor output to drive albedo.", failures);
    expect(graph.link(power, "value", pbr, "roughness"), testName, "Expected PowerFloat output to drive roughness.", failures);
    expect(graph.link(clamp, "value", pbr, "emission"), testName, "Expected ClampFloat output to drive emission.", failures);
    expect(!graph.link(ramp, "color", pbr, "roughness"), testName, "Expected ColorRamp output to reject Float input.", failures);
    expect(!graph.link(clamp, "value", pbr, "albedo"), testName, "Expected ClampFloat output to reject Color input.", failures);
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

void testValueNoisePresetUsesFactorGraph(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph value noise preset uses factor graph";
    sdf3d::SdfMaterial material;
    material.type = sdf3d::SdfMaterialType::ValueNoise;
    material.patternScale = 7.0f;
    material.albedo = {0.1f, 0.2f, 0.3f};
    material.secondaryAlbedo = {0.8f, 0.7f, 0.6f};

    const sdf3d::MaterialGraph graph = sdf3d::makeMaterialGraphFromMaterial(material);

    expect(countNodesOfType(graph, sdf3d::MaterialGraphNodeType::ValueNoise) == 1, testName, "Expected one active ValueNoise node.", failures);
    expect(countNodesOfType(graph, sdf3d::MaterialGraphNodeType::MixColor) == 1, testName, "Expected one MixColor node.", failures);
    expect(countNodesOfType(graph, sdf3d::MaterialGraphNodeType::ValueNoisePattern) == 0, testName, "Expected no legacy ValueNoisePattern node.", failures);
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

void testCompilerEmitsMultiplyColor(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph compiler emits multiply color";
    sdf3d::SdfMaterial material;

    sdf3d::MaterialGraph graph;
    const sdf3d::MaterialGraphNodeId multiply = graph.createNode(sdf3d::MaterialGraphNodeType::MultiplyColor, "Multiply");
    const sdf3d::MaterialGraphNodeId pbr = graph.createNode(sdf3d::MaterialGraphNodeType::PbrMaterial, "PBR");
    if (sdf3d::MaterialGraphNode* node = graph.node(multiply)) {
        node->color = {0.8f, 0.7f, 0.6f};
        node->secondaryColor = {0.5f, 0.4f, 0.3f};
        node->value = 0.25f;
    }
    expect(graph.link(multiply, "color", pbr, "albedo"), testName, "Expected multiply color to link to albedo.", failures);
    expect(graph.link(pbr, "material", graph.outputNode(), "material"), testName, "Expected PBR output link.", failures);

    sdf3d::MaterialDefinition definition{6, "Multiply", material, graph};
    sdf3d::SdfCompileResult result;
    const std::string glsl = sdf3d::MaterialGraphCompiler{}.emitMaterialFunction(definition, result);

    expect(contains(glsl, "mix(vec3(0.800000, 0.700000, 0.600000), (vec3(0.800000, 0.700000, 0.600000) * vec3(0.500000, 0.400000, 0.300000)), clamp(0.250000, 0.0, 1.0))"),
        testName,
        "Expected multiply color expression.",
        failures);
}

void testCompilerEmitsValueNoiseFloat(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph compiler emits value noise float";
    sdf3d::SdfMaterial material;

    sdf3d::MaterialGraph graph;
    const sdf3d::MaterialGraphNodeId noise = graph.createNode(sdf3d::MaterialGraphNodeType::ValueNoise, "Noise");
    const sdf3d::MaterialGraphNodeId pbr = graph.createNode(sdf3d::MaterialGraphNodeType::PbrMaterial, "PBR");
    if (sdf3d::MaterialGraphNode* node = graph.node(noise)) {
        node->value = 12.0f;
    }
    expect(graph.link(noise, "factor", pbr, "roughness"), testName, "Expected noise to drive roughness.", failures);
    expect(graph.link(pbr, "material", graph.outputNode(), "material"), testName, "Expected PBR output link.", failures);

    sdf3d::MaterialDefinition definition{7, "Noise", material, graph};
    sdf3d::SdfCompileResult result;
    const std::string glsl = sdf3d::MaterialGraphCompiler{}.emitMaterialFunction(definition, result);

    expect(contains(glsl, "clamp(sdf3d_valueNoise3d(p * max(12.000000, 0.0001)), 0.02, 1.0)"),
        testName,
        "Expected value noise expression through Float path.",
        failures);
}

void testCompilerEmitsCheckerFactor(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph compiler emits checker factor";
    sdf3d::SdfMaterial material;

    sdf3d::MaterialGraph graph;
    const sdf3d::MaterialGraphNodeId checker = graph.createNode(sdf3d::MaterialGraphNodeType::CheckerPattern, "Checker");
    const sdf3d::MaterialGraphNodeId pbr = graph.createNode(sdf3d::MaterialGraphNodeType::PbrMaterial, "PBR");
    if (sdf3d::MaterialGraphNode* node = graph.node(checker)) {
        node->value = 6.0f;
    }
    expect(graph.link(checker, "factor", pbr, "roughness"), testName, "Expected checker factor to drive roughness.", failures);
    expect(graph.link(pbr, "material", graph.outputNode(), "material"), testName, "Expected PBR output link.", failures);

    sdf3d::MaterialDefinition definition{10, "CheckerFactor", material, graph};
    sdf3d::SdfCompileResult result;
    const std::string glsl = sdf3d::MaterialGraphCompiler{}.emitMaterialFunction(definition, result);

    expect(contains(glsl, "clamp(mod(floor(p.x * max(6.000000, 0.0001)) + floor(p.y * max(6.000000, 0.0001)) + floor(p.z * max(6.000000, 0.0001)), 2.0), 0.02, 1.0)"),
        testName,
        "Expected checker expression through Float path.",
        failures);
}

void testCompilerEmitsCoreProceduralNodes(std::vector<TestFailure>& failures)
{
    const std::string testName = "material graph compiler emits core procedural nodes";
    sdf3d::SdfMaterial material;

    sdf3d::MaterialGraph graph;
    const sdf3d::MaterialGraphNodeId noise = graph.createNode(sdf3d::MaterialGraphNodeType::ValueNoise, "Noise");
    const sdf3d::MaterialGraphNodeId ramp = graph.createNode(sdf3d::MaterialGraphNodeType::ColorRamp, "Ramp");
    const sdf3d::MaterialGraphNodeId add = graph.createNode(sdf3d::MaterialGraphNodeType::AddColor, "Add");
    const sdf3d::MaterialGraphNodeId subtract = graph.createNode(sdf3d::MaterialGraphNodeType::SubtractColor, "Subtract");
    const sdf3d::MaterialGraphNodeId power = graph.createNode(sdf3d::MaterialGraphNodeType::PowerFloat, "Power");
    const sdf3d::MaterialGraphNodeId clamp = graph.createNode(sdf3d::MaterialGraphNodeType::ClampFloat, "Clamp");
    const sdf3d::MaterialGraphNodeId pbr = graph.createNode(sdf3d::MaterialGraphNodeType::PbrMaterial, "PBR");
    if (sdf3d::MaterialGraphNode* node = graph.node(noise)) {
        node->value = 5.0f;
    }
    if (sdf3d::MaterialGraphNode* node = graph.node(ramp)) {
        node->color = {0.1f, 0.2f, 0.3f};
        node->secondaryColor = {0.8f, 0.7f, 0.6f};
    }
    if (sdf3d::MaterialGraphNode* node = graph.node(add)) {
        node->color = {0.2f, 0.2f, 0.2f};
        node->secondaryColor = {0.3f, 0.3f, 0.3f};
    }
    if (sdf3d::MaterialGraphNode* node = graph.node(subtract)) {
        node->color = {0.9f, 0.8f, 0.7f};
        node->secondaryColor = {0.1f, 0.2f, 0.3f};
    }
    if (sdf3d::MaterialGraphNode* node = graph.node(power)) {
        node->value = 0.5f;
        node->secondaryValue = 2.0f;
    }
    if (sdf3d::MaterialGraphNode* node = graph.node(clamp)) {
        node->value = 1.5f;
        node->secondaryValue = 0.1f;
        node->tertiaryValue = 0.9f;
    }
    expect(graph.link(noise, "factor", ramp, "factor"), testName, "Expected noise to ramp link.", failures);
    expect(graph.link(ramp, "color", pbr, "albedo"), testName, "Expected ramp to albedo link.", failures);
    expect(graph.link(power, "value", pbr, "roughness"), testName, "Expected power to roughness link.", failures);
    expect(graph.link(clamp, "value", pbr, "emission"), testName, "Expected clamp to emission link.", failures);
    expect(graph.link(pbr, "material", graph.outputNode(), "material"), testName, "Expected PBR output link.", failures);

    sdf3d::MaterialDefinition definition{8, "Procedural", material, graph};
    sdf3d::SdfCompileResult result;
    const std::string glsl = sdf3d::MaterialGraphCompiler{}.emitMaterialFunction(definition, result);

    expect(contains(glsl, "mix(vec3(0.100000, 0.200000, 0.300000), vec3(0.800000, 0.700000, 0.600000), clamp(sdf3d_valueNoise3d(p * max(5.000000, 0.0001)), 0.0, 1.0))"),
        testName,
        "Expected ColorRamp GLSL.",
        failures);
    expect(contains(glsl, "pow(max(0.500000, 0.0), 2.000000)"), testName, "Expected PowerFloat GLSL.", failures);
    expect(contains(glsl, "clamp(1.500000, 0.100000, 0.900000)"), testName, "Expected ClampFloat GLSL.", failures);

    sdf3d::MaterialGraph colorGraph;
    const sdf3d::MaterialGraphNodeId addNode = colorGraph.createNode(sdf3d::MaterialGraphNodeType::AddColor, "Add");
    const sdf3d::MaterialGraphNodeId subtractNode = colorGraph.createNode(sdf3d::MaterialGraphNodeType::SubtractColor, "Subtract");
    const sdf3d::MaterialGraphNodeId colorPbr = colorGraph.createNode(sdf3d::MaterialGraphNodeType::PbrMaterial, "PBR");
    if (sdf3d::MaterialGraphNode* node = colorGraph.node(addNode)) {
        node->color = {0.2f, 0.2f, 0.2f};
        node->secondaryColor = {0.3f, 0.3f, 0.3f};
    }
    if (sdf3d::MaterialGraphNode* node = colorGraph.node(subtractNode)) {
        node->color = {0.9f, 0.8f, 0.7f};
        node->secondaryColor = {0.1f, 0.2f, 0.3f};
    }
    expect(colorGraph.link(addNode, "color", subtractNode, "a"), testName, "Expected AddColor to SubtractColor link.", failures);
    expect(colorGraph.link(subtractNode, "color", colorPbr, "albedo"), testName, "Expected SubtractColor to PBR link.", failures);
    expect(colorGraph.link(colorPbr, "material", colorGraph.outputNode(), "material"), testName, "Expected color graph output link.", failures);
    sdf3d::MaterialDefinition colorDefinition{9, "ColorOps", material, colorGraph};
    sdf3d::SdfCompileResult colorResult;
    const std::string colorGlsl = sdf3d::MaterialGraphCompiler{}.emitMaterialFunction(colorDefinition, colorResult);
    expect(contains(colorGlsl, "clamp((vec3(0.200000, 0.200000, 0.200000) + vec3(0.300000, 0.300000, 0.300000)), vec3(0.0), vec3(1.0))"),
        testName,
        "Expected AddColor GLSL.",
        failures);
    expect(contains(colorGlsl, "clamp((clamp((vec3(0.200000, 0.200000, 0.200000) + vec3(0.300000, 0.300000, 0.300000)), vec3(0.0), vec3(1.0)) - vec3(0.100000, 0.200000, 0.300000)), vec3(0.0), vec3(1.0))"),
        testName,
        "Expected SubtractColor GLSL.",
        failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;
    testCreateLinkDeleteSelect(failures);
    testLinkCompatibilityUsesSocketType(failures);
    testValueNoiseIsReusableFloat(failures);
    testCheckerOutputsColorAndFactor(failures);
    testCoreProceduralNodesLinkByType(failures);
    testRejectCycle(failures);
    testReplaceDataPreservesStableIds(failures);
    testCompilerEmitsPbrGraph(failures);
    testCompilerUsesProceduralPoint(failures);
    testValueNoisePresetUsesFactorGraph(failures);
    testCompilerUsesEmbeddedDefaultsAndLinkOverrides(failures);
    testCompilerEmitsMultiplyColor(failures);
    testCompilerEmitsValueNoiseFloat(failures);
    testCompilerEmitsCheckerFactor(failures);
    testCompilerEmitsCoreProceduralNodes(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All MaterialGraph tests passed.\n";
    return 0;
}
