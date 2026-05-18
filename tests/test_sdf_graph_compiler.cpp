#include "sdf3d/scene/SdfCompiler.h"
#include "sdf3d/scene/SdfGraph.h"

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

void testGraphCompilerOutputNode(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler output node";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    if (sdf3d::SdfGraphNode* node = graph.node(sphere)) {
        node->payload.parameters["radius"] = 1.5f;
    }
    const sdf3d::SdfGraphNodeId output = graph.outputNode();
    graph.link(sphere, "sdf", output, "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "length(p) - 1.500000"), testName, "Expected output node to compile linked surface.", failures);
    expect(result.materials.size() == 1, testName, "Expected linked surface material.", failures);
}

void testGraphCompilerUnlinkedOutputNode(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler unlinked output node";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(!result.errors.empty(), testName, "Expected unlinked output compiler error.", failures);
    expect(contains(result.errors.front(), "Output node"), testName, "Expected output node error text.", failures);
    expect(contains(result.glsl, "return 1e6;"), testName, "Expected safe no-hit GLSL.", failures);
}

void testGraphCompilerEmpty(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler empty";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfGraph graph;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(!result.errors.empty(), testName, "Expected empty graph compiler warning.", failures);
    expect(contains(result.glsl, "sceneMaterial"), testName, "Expected deferred material empty graph output.", failures);
    expect(contains(result.glsl, "return 1e6;"), testName, "Expected empty graph no-hit distance.", failures);
}

void testGraphCompilerPrimitive(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler primitive";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    if (sdf3d::SdfGraphNode* node = graph.node(sphere)) {
        node->payload.parameters["radius"] = 2.0f;
    }
    graph.link(sphere, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "length(p) - 2.000000"), testName, "Expected graph payload radius.", failures);
    expect(result.materials.size() == 1, testName, "Expected default graph material.", failures);
}

void testGraphCompilerMaterialOverride(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler material override";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    if (sdf3d::SdfGraphNode* node = graph.node(material)) {
        node->payload.material.albedo = {0.25f, 0.5f, 0.75f};
    }
    graph.link(sphere, "sdf", material, "sdf");
    graph.link(material, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.materials.size() == 2, testName, "Expected default plus graph material override.", failures);
    if (result.materials.size() == 2) {
        expect(result.materials[1].material.albedo.y == 0.5f, testName, "Expected graph material override payload.", failures);
    }
}

void testGraphNodeDefinitionMaterialOverride(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph material override definition";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    const sdf3d::SdfGraphNode* node = graph.node(material);

    expect(node != nullptr, testName, "Expected material override node.", failures);
    if (node != nullptr) {
        expect(node->inputs.size() == 1 && node->inputs[0].name == "sdf", testName, "Expected SDF input.", failures);
        expect(node->outputs.size() == 1 && node->outputs[0].name == "sdf", testName, "Expected SDF output.", failures);
    }
}

void testGraphCompilerLinkedTransform(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler linked transform";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    if (sdf3d::SdfGraphNode* node = graph.node(translate)) {
        node->payload.parameters["x"] = 3.0f;
        node->payload.parameters["y"] = 0.0f;
        node->payload.parameters["z"] = -1.0f;
    }
    graph.link(sphere, translate, "child");
    graph.link(translate, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "p - vec3(3.000000, 0.000000, -1.000000)"), testName, "Expected linked transform expression.", failures);
    expect(contains(result.glsl, "float sceneNodeSDF(int nodeId, vec3 p)"), testName, "Expected node highlight SDF entry point.", failures);
    expect(contains(result.glsl, "case "), testName, "Expected node highlight SDF switch cases.", failures);
}

void testGraphCompilerNonUniformScale(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler non-uniform scale";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId scale = graph.createNode(sdf3d::SdfNodeType::Scale, "Scale");
    if (sdf3d::SdfGraphNode* node = graph.node(scale)) {
        node->payload.parameters["x"] = 2.0f;
        node->payload.parameters["y"] = 3.0f;
        node->payload.parameters["z"] = 4.0f;
    }
    graph.link(sphere, scale, "child");
    graph.link(scale, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "p / vec3(2.000000, 3.000000, 4.000000)"), testName, "Expected non-uniform scaled point expression.", failures);
    expect(contains(result.glsl, "* 2.000000"), testName, "Expected min-axis distance rescale expression.", failures);
}

void testGraphCompilerCycle(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler cycle";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId a = graph.createNode(sdf3d::SdfNodeType::Union, "A");
    const sdf3d::SdfGraphNodeId b = graph.createNode(sdf3d::SdfNodeType::Union, "B");
    graph.link(a, b, "left");
    graph.link(b, a, "left");
    graph.link(a, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(!result.errors.empty(), testName, "Expected cycle compiler error.", failures);
    expect(contains(result.errors.front(), "Cycle"), testName, "Expected cycle error text.", failures);
}

void testGraphCompilerSocketOrdering(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler socket ordering";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId base = graph.createNode(sdf3d::SdfNodeType::Sphere, "Base");
    if (sdf3d::SdfGraphNode* node = graph.node(base)) {
        node->payload.parameters["radius"] = 2.0f;
    }

    const sdf3d::SdfGraphNodeId cutter = graph.createNode(sdf3d::SdfNodeType::Sphere, "Cutter");
    if (sdf3d::SdfGraphNode* node = graph.node(cutter)) {
        node->payload.parameters["radius"] = 0.5f;
    }

    const sdf3d::SdfGraphNodeId subtract = graph.createNode(sdf3d::SdfNodeType::Subtract, "Subtract");
    graph.link(cutter, subtract, "cutter");
    graph.link(base, subtract, "base");
    graph.link(subtract, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "max(-("), testName, "Expected subtract expression.", failures);

    const std::string baseCall = "sdf_node_" + std::to_string(base) + "(p)";
    const std::string cutterCall = "sdf_node_" + std::to_string(cutter) + "(p)";
    expect(contains(result.glsl, "length(p) - 2.000000"), testName, "Expected base radius expression.", failures);
    expect(contains(result.glsl, "length(p) - 0.500000"), testName, "Expected cutter radius expression.", failures);
    expect(contains(result.glsl, "max(-(" + cutterCall + "), " + baseCall + ")"), testName, "Expected cutter socket to be the negated subtract operand.", failures);
}

void testGraphCompilerIncompleteUnion(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler incomplete union";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");
    graph.link(unionNode, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(!result.errors.empty(), testName, "Expected incomplete union compiler error.", failures);
    expect(contains(result.glsl, "return 1e6;"), testName, "Expected geometry no-hit return.", failures);
    expect(contains(result.glsl, "return sampleMaterial(0);"), testName, "Expected default material return.", failures);
    expect(!contains(result.glsl, "sceneSDFWithMaterial"), testName, "Expected legacy material-aware sceneSDF removed.", failures);
}

void testGraphCompilerBypassSingleInputUnion(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler bypass single input union";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    if (sdf3d::SdfGraphNode* node = graph.node(sphere)) {
        node->payload.parameters["radius"] = 1.25f;
    }
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");
    graph.link(sphere, unionNode, "left");
    graph.link(unionNode, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected single-input union to bypass without errors.", failures);
    expect(contains(result.glsl, "length(p) - 1.250000"), testName, "Expected union to compile linked child.", failures);
}

void testGraphCompilerBypassInvalidUnionInput(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler bypass invalid union input";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    if (sdf3d::SdfGraphNode* node = graph.node(sphere)) {
        node->payload.parameters["radius"] = 1.25f;
    }
    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");
    graph.link(sphere, unionNode, "left");
    graph.link(translate, unionNode, "right");
    graph.link(unionNode, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(!result.errors.empty(), testName, "Expected invalid upstream input warning.", failures);
    expect(contains(result.glsl, "length(p) - 1.250000"), testName, "Expected union to bypass invalid input and keep valid child.", failures);
    expect(!contains(result.glsl, "min("), testName, "Expected union not to emit min with invalid input.", failures);
}

void testGraphCompilerBypassSubtractBase(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler bypass subtract base";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Base");
    if (sdf3d::SdfGraphNode* node = graph.node(sphere)) {
        node->payload.parameters["radius"] = 1.75f;
    }
    const sdf3d::SdfGraphNodeId subtract = graph.createNode(sdf3d::SdfNodeType::Subtract, "Subtract");
    graph.link(sphere, subtract, "base");
    graph.link(subtract, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(!result.errors.empty(), testName, "Expected missing cutter warning.", failures);
    expect(contains(result.glsl, "length(p) - 1.750000"), testName, "Expected subtract to bypass to base child.", failures);
    expect(!contains(result.glsl, "max(-("), testName, "Expected incomplete subtract not to emit subtract operation.", failures);
}

void testGraphCompilerMissingTransformChild(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler missing transform child";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId translate = graph.createNode(sdf3d::SdfNodeType::Translate, "Translate");
    graph.link(translate, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(!result.errors.empty(), testName, "Expected missing transform child warning.", failures);
    expect(contains(result.glsl, "return 1e6;"), testName, "Expected no-hit distance for missing child.", failures);
}

void testGraphCompilerRepeat(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler repeat";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId repeat = graph.createNode(sdf3d::SdfNodeType::Repeat, "Repeat");
    if (sdf3d::SdfGraphNode* node = graph.node(repeat)) {
        node->payload.parameters["x"] = 3.0f;
        node->payload.parameters["y"] = 4.0f;
        node->payload.parameters["z"] = 5.0f;
        node->payload.parameters["repeatY"] = 0.0f;
    }
    graph.link(sphere, repeat, "child");
    graph.link(repeat, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "vec3((mod(p.x + 0.5 * 3.000000, 3.000000) - 0.5 * 3.000000), p.y, (mod(p.z + 0.5 * 5.000000, 5.000000) - 0.5 * 5.000000))"), testName, "Expected repeat to skip disabled y axis.", failures);
}

void testGraphCompilerRepeatAllAxesDefault(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler repeat all axes default";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId repeat = graph.createNode(sdf3d::SdfNodeType::Repeat, "Repeat");
    if (sdf3d::SdfGraphNode* node = graph.node(repeat)) {
        node->payload.parameters["x"] = 3.0f;
        node->payload.parameters["y"] = 4.0f;
        node->payload.parameters["z"] = 5.0f;
    }
    graph.link(sphere, repeat, "child");
    graph.link(repeat, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "mod(p + 0.5 * vec3(3.000000, 4.000000, 5.000000)"), testName, "Expected default repeat to affect all axes.", failures);
}

void testGraphCompilerMirror(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler mirror";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId mirror = graph.createNode(sdf3d::SdfNodeType::Mirror, "Mirror");
    if (sdf3d::SdfGraphNode* node = graph.node(mirror)) {
        node->payload.parameters["x"] = 1.0f;
        node->payload.parameters["y"] = 0.0f;
        node->payload.parameters["z"] = 1.0f;
    }
    graph.link(sphere, mirror, "child");
    graph.link(mirror, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "vec3(abs(p.x), p.y, abs(p.z))"), testName, "Expected mirror domain transform.", failures);
}

void testGraphCompilerTwist(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler twist";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId twist = graph.createNode(sdf3d::SdfNodeType::Twist, "Twist");
    if (sdf3d::SdfGraphNode* node = graph.node(twist)) {
        node->payload.parameters["strength"] = 1.25f;
        node->payload.parameters["axis"] = 2.0f;
    }
    graph.link(sphere, twist, "child");
    graph.link(twist, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "cos((p.z * 1.250000))"), testName, "Expected twist z-axis cosine angle.", failures);
    expect(contains(result.glsl, "sin((p.z * 1.250000))"), testName, "Expected twist z-axis sine angle.", failures);
    expect(contains(result.glsl, " / (1.0 + abs(1.250000) * 1.500000)"), testName, "Expected twist distance correction.", failures);
}

void testGraphCompilerBend(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler bend";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId bend = graph.createNode(sdf3d::SdfNodeType::Bend, "Bend");
    if (sdf3d::SdfGraphNode* node = graph.node(bend)) {
        node->payload.parameters["strength"] = 0.75f;
        node->payload.parameters["axis"] = 1.0f;
    }
    graph.link(sphere, bend, "child");
    graph.link(bend, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "cos((p.y * 0.750000))"), testName, "Expected bend y-axis cosine angle.", failures);
    expect(contains(result.glsl, "sin((p.y * 0.750000))"), testName, "Expected bend y-axis sine angle.", failures);
    expect(contains(result.glsl, " / (1.0 + abs(0.750000) * 1.500000)"), testName, "Expected bend distance correction.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testGraphCompilerOutputNode(failures);
    testGraphCompilerUnlinkedOutputNode(failures);
    testGraphCompilerEmpty(failures);
    testGraphCompilerPrimitive(failures);
    testGraphCompilerMaterialOverride(failures);
    testGraphNodeDefinitionMaterialOverride(failures);
    testGraphCompilerLinkedTransform(failures);
    testGraphCompilerNonUniformScale(failures);
    testGraphCompilerCycle(failures);
    testGraphCompilerSocketOrdering(failures);
    testGraphCompilerIncompleteUnion(failures);
    testGraphCompilerBypassSingleInputUnion(failures);
    testGraphCompilerBypassInvalidUnionInput(failures);
    testGraphCompilerBypassSubtractBase(failures);
    testGraphCompilerMissingTransformChild(failures);
    testGraphCompilerRepeat(failures);
    testGraphCompilerRepeatAllAxesDefault(failures);
    testGraphCompilerMirror(failures);
    testGraphCompilerTwist(failures);
    testGraphCompilerBend(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All SDF graph compiler tests passed.\n";
    return 0;
}
