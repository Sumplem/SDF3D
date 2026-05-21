#include "sdf3d/scene/SdfCompiler.h"
#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/scene/SdfGraph.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_set>
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

bool hasDuplicateSdfHelperNames(const std::string& glsl)
{
    std::unordered_set<std::string> names;
    std::size_t offset = 0;
    const std::string prefix = "float sdf_node_";
    while ((offset = glsl.find(prefix, offset)) != std::string::npos) {
        const std::size_t nameStart = offset + std::string("float ").size();
        const std::size_t nameEnd = glsl.find('(', nameStart);
        if (nameEnd == std::string::npos) {
            return true;
        }
        const std::string name = glsl.substr(nameStart, nameEnd - nameStart);
        if (!names.insert(name).second) {
            return true;
        }
        offset = nameEnd;
    }
    return false;
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
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::SolidMaterial, "Material");
    const sdf3d::SdfGraphNodeId materialOverride = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Override");
    if (sdf3d::SdfGraphNode* node = graph.node(material)) {
        if (sdf3d::MaterialDefinition* definition = graph.materials().material(node->payload.materialId)) {
            definition->material.albedo = {0.25f, 0.5f, 0.75f};
        }
    }
    graph.link(sphere, "sdf", materialOverride, "sdf");
    graph.link(material, "material", materialOverride, "material");
    graph.link(materialOverride, "sdf", graph.outputNode(), "surface");

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
        expect(node->inputs.size() == 2 && node->inputs[0].name == "sdf" && node->inputs[1].name == "material", testName, "Expected SDF and material inputs.", failures);
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
    expect(contains(result.glsl, "sdf3d_nodeParam0("), testName, "Expected runtime node param lookup.", failures);
    expect(contains(result.glsl, "vec4(3.000000, 0.000000, -1.000000, 0.000000)).xyz"), testName, "Expected linked transform fallback expression.", failures);
    expect(result.nodeParams.size() == 1, testName, "Expected transform node param.", failures);
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
    expect(contains(result.glsl, "vec4(2.000000, 3.000000, 4.000000, 2.000000)"), testName, "Expected non-uniform scale fallback params.", failures);
    expect(contains(result.glsl, ".xyz"), testName, "Expected runtime scale vector expression.", failures);
    expect(contains(result.glsl, ".w"), testName, "Expected runtime min-axis distance rescale expression.", failures);
    expect(result.nodeParams.size() == 1, testName, "Expected scale node param.", failures);
}

void testGraphCompilerCollectsGroupTransformParams(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler collects group transform params";
    sdf3d::SdfGraph groupGraph;
    const sdf3d::SdfGraphNodeId sphere = groupGraph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId translate = groupGraph.createNode(sdf3d::SdfNodeType::Translate, "Group Translate");
    if (sdf3d::SdfGraphNode* node = groupGraph.node(translate)) {
        node->payload.parameters["x"] = 2.0f;
        node->payload.parameters["y"] = 3.0f;
        node->payload.parameters["z"] = 4.0f;
    }
    expect(groupGraph.link(sphere, translate, "child"), testName, "Expected group translate child link.", failures);
    expect(groupGraph.link(translate, "sdf", groupGraph.outputNode(), "surface"), testName, "Expected group output link.", failures);

    sdf3d::GraphGroupRegistry groups;
    const sdf3d::GroupDefId definitionId = groups.createDefinition("Translated Group", groupGraph);

    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId groupInstance = graph.createNode(sdf3d::SdfNodeType::Group, "Group Instance");
    if (sdf3d::SdfGraphNode* node = graph.node(groupInstance)) {
        node->payload.groupDefinitionId = definitionId;
    }
    expect(graph.link(groupInstance, "sdf", graph.outputNode(), "surface"), testName, "Expected group instance output link.", failures);

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph, groups);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    const auto it = std::find_if(result.nodeParams.begin(), result.nodeParams.end(), [translate](const sdf3d::SdfCompiledNodeParam& param) {
        return param.nodeId != translate;
    });
    expect(it != result.nodeParams.end(), testName, "Expected group transform node param.", failures);
    if (it != result.nodeParams.end()) {
        expect(it->data0[0] == 2.0f && it->data0[1] == 3.0f && it->data0[2] == 4.0f, testName, "Expected group translate params packed.", failures);
    }
}

void testGraphCompilerNamespacesGroupHelperIds(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler namespaces group helper ids";
    sdf3d::SdfGraph groupGraph;
    const sdf3d::SdfGraphNodeId groupSphere = groupGraph.createNode(sdf3d::SdfNodeType::Sphere, "Group Sphere");
    expect(groupSphere == 2, testName, "Expected group sphere id to collide with root sphere id.", failures);
    expect(groupGraph.link(groupSphere, "sdf", groupGraph.outputNode(), "surface"), testName, "Expected group output link.", failures);

    sdf3d::GraphGroupRegistry groups;
    const sdf3d::GroupDefId definitionId = groups.createDefinition("Group", groupGraph);

    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId rootSphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Root Sphere");
    const sdf3d::SdfGraphNodeId groupInstance = graph.createNode(sdf3d::SdfNodeType::Group, "Group Instance");
    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");
    if (sdf3d::SdfGraphNode* node = graph.node(groupInstance)) {
        node->payload.groupDefinitionId = definitionId;
    }
    expect(rootSphere == groupSphere, testName, "Expected root and group node stable ids to collide before namespacing.", failures);
    expect(graph.link(rootSphere, "sdf", unionNode, "inputs"), testName, "Expected root sphere link.", failures);
    expect(graph.link(groupInstance, "sdf", unionNode, "inputs"), testName, "Expected group instance link.", failures);
    expect(graph.link(unionNode, "sdf", graph.outputNode(), "surface"), testName, "Expected output link.", failures);

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph, groups);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(!hasDuplicateSdfHelperNames(result.glsl), testName, "Expected no duplicate sdf_node helper function names.", failures);
}

void testGraphCompilerCycle(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler cycle";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId a = graph.createNode(sdf3d::SdfNodeType::Union, "A");
    const sdf3d::SdfGraphNodeId b = graph.createNode(sdf3d::SdfNodeType::Union, "B");
    graph.link(a, b, "inputs");
    graph.link(b, a, "inputs");
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
    expect(contains(result.glsl, "return sampleMaterial(0, p);"), testName, "Expected default material return.", failures);
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
    graph.link(sphere, unionNode, "inputs");
    graph.link(unionNode, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected single-input union to bypass without errors.", failures);
    expect(contains(result.glsl, "length(p) - 1.250000"), testName, "Expected union to compile linked child.", failures);
}

void testGraphCompilerMultiInputUnion(std::vector<TestFailure>& failures)
{
    const std::string testName = "graph compiler multi-input union";
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId first = graph.createNode(sdf3d::SdfNodeType::Sphere, "First");
    const sdf3d::SdfGraphNodeId second = graph.createNode(sdf3d::SdfNodeType::Sphere, "Second");
    const sdf3d::SdfGraphNodeId third = graph.createNode(sdf3d::SdfNodeType::Sphere, "Third");
    graph.node(first)->payload.parameters["radius"] = 1.0f;
    graph.node(second)->payload.parameters["radius"] = 2.0f;
    graph.node(third)->payload.parameters["radius"] = 3.0f;

    const sdf3d::SdfGraphNodeId unionNode = graph.createNode(sdf3d::SdfNodeType::Union, "Union");
    expect(graph.link(first, "sdf", unionNode, "inputs"), testName, "Expected first input link.", failures);
    expect(graph.link(second, "sdf", unionNode, "inputs"), testName, "Expected second input link.", failures);
    expect(graph.link(third, "sdf", unionNode, "inputs"), testName, "Expected third input link.", failures);
    expect(graph.link(unionNode, "sdf", graph.outputNode(), "surface"), testName, "Expected union output link.", failures);

    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(graph);

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(contains(result.glsl, "length(p) - 1.000000"), testName, "Expected first union child.", failures);
    expect(contains(result.glsl, "length(p) - 2.000000"), testName, "Expected second union child.", failures);
    expect(contains(result.glsl, "length(p) - 3.000000"), testName, "Expected third union child.", failures);
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
    graph.link(sphere, unionNode, "inputs");
    graph.link(translate, unionNode, "inputs");
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
    testGraphCompilerCollectsGroupTransformParams(failures);
    testGraphCompilerNamespacesGroupHelperIds(failures);
    testGraphCompilerCycle(failures);
    testGraphCompilerSocketOrdering(failures);
    testGraphCompilerIncompleteUnion(failures);
    testGraphCompilerBypassSingleInputUnion(failures);
    testGraphCompilerMultiInputUnion(failures);
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
