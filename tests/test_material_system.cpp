#include "sdf3d/systems/MaterialSystem.h"

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

void expect(bool condition, const std::string& testName, const std::string& message, std::vector<TestFailure>& failures)
{
    if (!condition) {
        failures.push_back({testName, message});
    }
}

void testAppendOrder(std::vector<TestFailure>& failures)
{
    const std::string testName = "append order";
    sdf3d::MaterialSystem materials;
    sdf3d::SdfCompileResult result;
    sdf3d::SdfMaterial red;
    red.albedo = {1.0f, 0.0f, 0.0f};
    sdf3d::SdfMaterial blue;
    blue.albedo = {0.0f, 0.0f, 1.0f};

    const int defaultId = materials.ensureDefaultMaterial(result);
    const int redId = materials.appendMaterial(result, red);
    const int blueId = materials.appendMaterial(result, blue);

    expect(defaultId == 0, testName, "Expected default material ID 0.", failures);
    expect(redId == 1, testName, "Expected first override material ID 1.", failures);
    expect(blueId == 2, testName, "Expected second override material ID 2.", failures);
    expect(result.materials.size() == 3, testName, "Expected default plus two packed materials.", failures);
    if (result.materials.size() == 3) {
        expect(result.materials[1].material.albedo.x == 1.0f, testName, "Expected first material preserved.", failures);
        expect(result.materials[2].material.albedo.z == 1.0f, testName, "Expected second material preserved.", failures);
    }
}

void testCollectTreeMaterials(std::vector<TestFailure>& failures)
{
    const std::string testName = "collect tree materials";
    sdf3d::SdfNodePtr red = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Red");
    red->material.albedo = {1.0f, 0.0f, 0.0f};
    red->children.push_back(sdf3d::makeSphereNode());

    sdf3d::SdfNodePtr blue = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Blue");
    blue->material.albedo = {0.0f, 0.0f, 1.0f};
    blue->children.push_back(sdf3d::makeBoxNode());

    const sdf3d::SdfCompileResult result = sdf3d::MaterialSystem{}.collectMaterials(sdf3d::makeUnionNode({red, blue}));

    expect(result.errors.empty(), testName, "Expected no material collection errors.", failures);
    expect(result.materials.size() == 3, testName, "Expected default plus two material overrides.", failures);
    if (result.materials.size() == 3) {
        expect(result.materials[1].material.albedo.x == 1.0f, testName, "Expected red material first.", failures);
        expect(result.materials[2].material.albedo.z == 1.0f, testName, "Expected blue material second.", failures);
    }
}

void testCollectGraphMaterials(std::vector<TestFailure>& failures)
{
    const std::string testName = "collect graph materials";
    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere");
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    if (sdf3d::SdfGraphNode* node = graph.node(material)) {
        node->payload.material.emission = 2.0f;
    }
    graph.link(sphere, "sdf", material, "sdf");
    graph.link(material, "sdf", graph.outputNode(), "surface");

    const sdf3d::SdfCompileResult result = sdf3d::MaterialSystem{}.collectMaterials(graph);

    expect(result.errors.empty(), testName, "Expected no graph material collection errors.", failures);
    expect(result.materials.size() == 2, testName, "Expected default plus graph material override.", failures);
    if (result.materials.size() == 2) {
        expect(result.materials[1].material.emission == 2.0f, testName, "Expected graph material value preserved.", failures);
    }
    expect(result.glsl.empty(), testName, "Expected material collection not to emit GLSL.", failures);
}

void testCollectSmoothSubtractMaterials(std::vector<TestFailure>& failures)
{
    const std::string testName = "collect smooth subtract materials";
    sdf3d::SdfNodePtr baseMaterial = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Base Material");
    baseMaterial->material.albedo = {1.0f, 0.0f, 0.0f};
    baseMaterial->children.push_back(sdf3d::makeSphereNode());

    sdf3d::SdfNodePtr cutterMaterial = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Cutter Material");
    cutterMaterial->material.albedo = {0.0f, 1.0f, 0.0f};
    cutterMaterial->children.push_back(sdf3d::makeBoxNode());

    const sdf3d::SdfCompileResult result = sdf3d::MaterialSystem{}.collectMaterials(
        sdf3d::makeSmoothSubtractNode(baseMaterial, cutterMaterial, 0.25f)
    );

    expect(result.errors.empty(), testName, "Expected no smooth subtract material collection errors.", failures);
    expect(result.materials.size() == 3, testName, "Expected base and cutter materials for smooth subtract blending.", failures);
    if (result.materials.size() == 3) {
        expect(result.materials[1].material.albedo.x == 1.0f, testName, "Expected base material first.", failures);
        expect(result.materials[2].material.albedo.y == 1.0f, testName, "Expected cutter material second.", failures);
    }
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testAppendOrder(failures);
    testCollectTreeMaterials(failures);
    testCollectGraphMaterials(failures);
    testCollectSmoothSubtractMaterials(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All MaterialSystem tests passed.\n";
    return 0;
}
