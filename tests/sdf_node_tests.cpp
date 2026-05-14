#include "sdf3d/scene/SdfCompiler.h"
#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/scene/SdfNode.h"

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

void testDefaultSphere(std::vector<TestFailure>& failures)
{
    const std::string testName = "default sphere";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(sdf3d::makeSphereNode());

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.materials.size() == 1, testName, "Expected one compiled material.", failures);
    expect(contains(result.glsl, "float sceneSDF(vec3 p)"), testName, "Expected sceneSDF declaration.", failures);
    expect(contains(result.glsl, "vec2 sceneSDFWithMaterial(vec3 p)"), testName, "Expected material-aware sceneSDF declaration.", failures);
    expect(contains(result.glsl, "length(p) - 1.000000"), testName, "Expected unit sphere expression.", failures);
    expect(!result.usesBox, testName, "Expected box helper to be unused.", failures);
    expect(!result.usesCylinder, testName, "Expected cylinder helper to be unused.", failures);
    expect(!result.usesSmoothMin, testName, "Expected smooth-min helper to be unused.", failures);
    expect(!result.usesRotate, testName, "Expected rotation helper to be unused.", failures);
    expect(!contains(result.glsl, "sdf3d_box"), testName, "Expected box helper to be omitted.", failures);
    expect(!contains(result.glsl, "sdf3d_cylinder"), testName, "Expected cylinder helper to be omitted.", failures);
    expect(!contains(result.glsl, "sdf3d_smin"), testName, "Expected smooth-min helper to be omitted.", failures);
    expect(!contains(result.glsl, "sdf3d_rotationXYZ"), testName, "Expected rotation helper to be omitted.", failures);
}

void testEmptyScene(std::vector<TestFailure>& failures)
{
    const std::string testName = "empty scene";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(nullptr);

    expect(!result.errors.empty(), testName, "Expected an empty-scene compiler warning.", failures);
    expect(contains(result.glsl, "return 1e6;"), testName, "Expected no-hit fallback distance.", failures);
}

void testDeterministicOutput(std::vector<TestFailure>& failures)
{
    const std::string testName = "deterministic output";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfNodePtr node = sdf3d::makeTranslateNode(sdf3d::makeSphereNode(), {1.0f, 2.0f, 3.0f});

    const sdf3d::SdfCompileResult first = compiler.compile(node);
    const sdf3d::SdfCompileResult second = compiler.compile(node);

    expect(first.glsl == second.glsl, testName, "Expected repeated compilation to produce identical GLSL.", failures);
}

void testTranslate(std::vector<TestFailure>& failures)
{
    const std::string testName = "translate";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(
        sdf3d::makeTranslateNode(sdf3d::makeSphereNode(), {1.0f, -2.0f, 0.5f})
    );

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "p - vec3(1.000000, -2.000000, 0.500000)"), testName, "Expected translated point expression.", failures);
}

void testScale(std::vector<TestFailure>& failures)
{
    const std::string testName = "scale";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(
        sdf3d::makeScaleNode(sdf3d::makeSphereNode(), 2.0f)
    );

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "p / 2.000000"), testName, "Expected scaled point expression.", failures);
    expect(contains(result.glsl, "* 2.000000"), testName, "Expected distance rescale expression.", failures);
}

void testUnion(std::vector<TestFailure>& failures)
{
    const std::string testName = "union";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(
        sdf3d::makeUnionNode({
            sdf3d::makeSphereNode("A"),
            sdf3d::makeTranslateNode(sdf3d::makeSphereNode("B"), {2.0f, 0.0f, 0.0f}),
        })
    );

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "min("), testName, "Expected union to emit min().", failures);
    expect(contains(result.glsl, "vec3(2.000000, 0.000000, 0.000000)"), testName, "Expected translated child expression.", failures);
}

void testBox(std::vector<TestFailure>& failures)
{
    const std::string testName = "box";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(sdf3d::makeBoxNode({1.0f, 2.0f, 3.0f}));

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.usesBox, testName, "Expected box helper flag.", failures);
    expect(contains(result.glsl, "float sdf3d_box"), testName, "Expected box helper.", failures);
    expect(contains(result.glsl, "sdf3d_box(p, vec3(1.000000, 2.000000, 3.000000))"), testName, "Expected box helper call.", failures);
}

void testCylinder(std::vector<TestFailure>& failures)
{
    const std::string testName = "cylinder";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(sdf3d::makeCylinderNode(0.5f, 2.0f));

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.usesCylinder, testName, "Expected cylinder helper flag.", failures);
    expect(contains(result.glsl, "float sdf3d_cylinder"), testName, "Expected cylinder helper.", failures);
    expect(contains(result.glsl, "sdf3d_cylinder(p, 0.500000, 2.000000)"), testName, "Expected cylinder helper call.", failures);
}

void testTorus(std::vector<TestFailure>& failures)
{
    const std::string testName = "torus";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(sdf3d::makeTorusNode(1.5f, 0.25f));

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "length(p.xz) - 1.500000"), testName, "Expected torus major radius expression.", failures);
    expect(contains(result.glsl, "- 0.250000"), testName, "Expected torus minor radius expression.", failures);
}

void testPlane(std::vector<TestFailure>& failures)
{
    const std::string testName = "plane";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(sdf3d::makePlaneNode({0.0f, 1.0f, 0.0f}, -1.0f));

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "dot(p, normalize(vec3(0.000000, 1.000000, 0.000000)))"), testName, "Expected plane normal expression.", failures);
    expect(contains(result.glsl, "+ -1.000000"), testName, "Expected plane offset expression.", failures);
}

void testSubtract(std::vector<TestFailure>& failures)
{
    const std::string testName = "subtract";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(
        sdf3d::makeSubtractNode(sdf3d::makeSphereNode("Base"), sdf3d::makeBoxNode({0.5f, 0.5f, 0.5f}, "Cutter"))
    );

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "max(-("), testName, "Expected subtract to negate the cutter expression.", failures);
    expect(contains(result.glsl, "sdf3d_box(p, vec3(0.500000, 0.500000, 0.500000))"), testName, "Expected cutter expression.", failures);
}

void testIntersect(std::vector<TestFailure>& failures)
{
    const std::string testName = "intersect";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(
        sdf3d::makeIntersectNode({
            sdf3d::makeSphereNode("A"),
            sdf3d::makeBoxNode({1.0f, 1.0f, 1.0f}, "B"),
        })
    );

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "max("), testName, "Expected intersect to emit max().", failures);
    expect(contains(result.glsl, "length(p) - 1.000000"), testName, "Expected first child expression.", failures);
}

void testSmoothUnion(std::vector<TestFailure>& failures)
{
    const std::string testName = "smooth union";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(
        sdf3d::makeSmoothUnionNode({
            sdf3d::makeSphereNode("A"),
            sdf3d::makeBoxNode({1.0f, 1.0f, 1.0f}, "B"),
        }, 0.4f)
    );

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.usesSmoothMin, testName, "Expected smooth-min helper flag.", failures);
    expect(contains(result.glsl, "float sdf3d_smin"), testName, "Expected smooth-min helper.", failures);
    expect(contains(result.glsl, "sdf3d_smin("), testName, "Expected smooth union call.", failures);
    expect(contains(result.glsl, "0.400000"), testName, "Expected smoothness parameter.", failures);
}

void testSmoothSubtract(std::vector<TestFailure>& failures)
{
    const std::string testName = "smooth subtract";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(
        sdf3d::makeSmoothSubtractNode(sdf3d::makeSphereNode("Base"), sdf3d::makeBoxNode({0.5f, 0.5f, 0.5f}, "Cutter"), 0.2f)
    );

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.usesSmoothMin, testName, "Expected smooth-min helper flag.", failures);
    expect(contains(result.glsl, "-sdf3d_smin"), testName, "Expected smooth subtract expression.", failures);
    expect(contains(result.glsl, "0.200000"), testName, "Expected smoothness parameter.", failures);
}

void testSmoothIntersect(std::vector<TestFailure>& failures)
{
    const std::string testName = "smooth intersect";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(
        sdf3d::makeSmoothIntersectNode({
            sdf3d::makeSphereNode("A"),
            sdf3d::makeBoxNode({1.0f, 1.0f, 1.0f}, "B"),
        }, 0.3f)
    );

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.usesSmoothMin, testName, "Expected smooth-min helper flag.", failures);
    expect(contains(result.glsl, "-sdf3d_smin"), testName, "Expected smooth intersect expression.", failures);
    expect(contains(result.glsl, "0.300000"), testName, "Expected smoothness parameter.", failures);
}

void testRotate(std::vector<TestFailure>& failures)
{
    const std::string testName = "rotate";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(
        sdf3d::makeRotateNode(sdf3d::makeBoxNode({1.0f, 2.0f, 3.0f}), {15.0f, 30.0f, 45.0f})
    );

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.usesRotate, testName, "Expected rotation helper flag.", failures);
    expect(contains(result.glsl, "mat3 sdf3d_rotationXYZ"), testName, "Expected rotation helper.", failures);
    expect(contains(result.glsl, "transpose(sdf3d_rotationXYZ(vec3(15.000000, 30.000000, 45.000000)))"), testName, "Expected inverse rotation expression.", failures);
}

void testMaterialMetadata(std::vector<TestFailure>& failures)
{
    const std::string testName = "material metadata";
    const sdf3d::SdfCompiler compiler;

    sdf3d::SdfNodePtr redSphere = sdf3d::makeSphereNode("Red");
    redSphere->material.albedo = {1.0f, 0.0f, 0.0f};
    redSphere->material.emission = 0.5f;

    sdf3d::SdfNodePtr blueBox = sdf3d::makeBoxNode({1.0f, 1.0f, 1.0f}, "Blue");
    blueBox->material.albedo = {0.0f, 0.0f, 1.0f};
    blueBox->material.roughness = 0.25f;

    const sdf3d::SdfCompileResult result = compiler.compile(sdf3d::makeUnionNode({redSphere, blueBox}));

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.materials.size() == 2, testName, "Expected one material per primitive leaf.", failures);
    if (result.materials.size() == 2) {
        // AGENT: Compile order is depth-first, so material IDs are stable for
        // shader uniforms and testable without renderer involvement.
        expect(result.materials[0].material.albedo.x == 1.0f, testName, "Expected first material to be red sphere.", failures);
        expect(result.materials[0].material.emission == 0.5f, testName, "Expected first material emission to be preserved.", failures);
        expect(result.materials[1].material.albedo.z == 1.0f, testName, "Expected second material to be blue box.", failures);
        expect(result.materials[1].material.roughness == 0.25f, testName, "Expected second material roughness to be preserved.", failures);
    }
    expect(contains(result.glsl, "vec2("), testName, "Expected compiled GLSL to emit material hit records.", failures);
}

void testGraphCreateSelectOutput(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph create select output";
    sdf3d::SdfGraph graph;

    expect(graph.outputNode() == 0, testName, "Expected empty graph output.", failures);
    expect(graph.selectedNode() == 0, testName, "Expected empty graph selection.", failures);

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");

    expect(sphere != 0, testName, "Expected nonzero graph node ID.", failures);
    expect(graph.outputNode() == sphere, testName, "Expected first node to become output.", failures);
    expect(graph.selectedNode() == sphere, testName, "Expected created node to become selected.", failures);
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

    // AGENT: One input socket accepts one source in the first graph model; a
    // later multi-input operation can add indexed socket names.
    const sdf3d::SdfGraphNodeId box = graph.createNode(sdf3d::SdfNodeType::Box, "Box");
    expect(graph.link(box, translate, "child"), testName, "Expected replacement link to succeed.", failures);
    expect(graph.links().size() == 1, testName, "Expected socket link replacement.", failures);
    expect(graph.links().front().fromNode == box, testName, "Expected replacement source node.", failures);

    expect(graph.deleteNode(box), testName, "Expected delete to succeed.", failures);
    expect(graph.links().empty(), testName, "Expected connected links to be removed.", failures);
    expect(graph.node(box) == nullptr, testName, "Expected deleted node lookup to fail.", failures);
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

void testGraphOutputAndSelectionValidation(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph output and selection validation";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");

    expect(!graph.setOutputNode(9999), testName, "Expected invalid output node to fail.", failures);
    expect(!graph.setSelectedNode(9999), testName, "Expected invalid selected node to fail.", failures);
    expect(graph.setOutputNode(0), testName, "Expected clearing output to succeed.", failures);
    expect(graph.outputNode() == 0, testName, "Expected output to be cleared.", failures);
    expect(graph.setSelectedNode(0), testName, "Expected clearing selection to succeed.", failures);
    expect(graph.selectedNode() == 0, testName, "Expected selection to be cleared.", failures);
    expect(graph.setOutputNode(sphere), testName, "Expected valid output node to succeed.", failures);
    expect(graph.outputNode() == sphere, testName, "Expected output to be restored.", failures);
}

void testGraphCompilerEmpty(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler empty";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfGraph graph;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(!result.errors.empty(), testName, "Expected empty graph compiler warning.", failures);
    expect(contains(result.glsl, "sceneSDFWithMaterial"), testName, "Expected material-aware empty graph output.", failures);
    expect(contains(result.glsl, "return 1e6;"), testName, "Expected empty graph no-hit distance.", failures);
}

void testGraphCompilerPrimitive(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler primitive";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    if (sdf3d::SdfGraphNode* node = graph.node(sphere)) {
        node->payload.parameters["radius"] = 2.0f;
        node->payload.material.albedo = {0.25f, 0.5f, 0.75f};
    }

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "length(p) - 2.000000"), testName, "Expected graph payload radius.", failures);
    expect(result.materials.size() == 1, testName, "Expected one graph material.", failures);
    if (result.materials.size() == 1) {
        expect(result.materials[0].material.albedo.y == 0.5f, testName, "Expected graph payload material.", failures);
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
    graph.setOutputNode(translate);

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "p - vec3(3.000000, 0.000000, -1.000000)"), testName, "Expected linked transform expression.", failures);
}

void testGraphCompilerCycle(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler cycle";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId a = graph.createNode(sdf3d::SdfNodeType::Union, "A");
    const sdf3d::SdfGraphNodeId b = graph.createNode(sdf3d::SdfNodeType::Union, "B");
    graph.link(a, b, "left");
    graph.link(b, a, "left");
    graph.setOutputNode(a);

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

    // AGENT: Links are added in reverse logical order to prove named sockets,
    // not insertion order, control binary operation operands.
    graph.link(cutter, subtract, "cutter");
    graph.link(base, subtract, "base");
    graph.setOutputNode(subtract);

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "max(-("), testName, "Expected subtract expression.", failures);

    const size_t basePosition = result.glsl.find("length(p) - 2.000000");
    const size_t cutterPosition = result.glsl.find("length(p) - 0.500000");
    expect(basePosition != std::string::npos, testName, "Expected base radius expression.", failures);
    expect(cutterPosition != std::string::npos, testName, "Expected cutter radius expression.", failures);
    expect(cutterPosition < basePosition, testName, "Expected cutter socket to be the negated subtract operand.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testDefaultSphere(failures);
    testEmptyScene(failures);
    testDeterministicOutput(failures);
    testTranslate(failures);
    testScale(failures);
    testUnion(failures);
    testBox(failures);
    testCylinder(failures);
    testTorus(failures);
    testPlane(failures);
    testSubtract(failures);
    testIntersect(failures);
    testSmoothUnion(failures);
    testSmoothSubtract(failures);
    testSmoothIntersect(failures);
    testRotate(failures);
    testMaterialMetadata(failures);
    testGraphCreateSelectOutput(failures);
    testGraphLinksAndDelete(failures);
    testGraphSocketsAndTypedLinks(failures);
    testGraphOutputAndSelectionValidation(failures);
    testGraphCompilerEmpty(failures);
    testGraphCompilerPrimitive(failures);
    testGraphCompilerLinkedTransform(failures);
    testGraphCompilerCycle(failures);
    testGraphCompilerSocketOrdering(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All SDF node tests passed.\n";
    return 0;
}
