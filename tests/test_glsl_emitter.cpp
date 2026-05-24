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

size_t findHelper(const sdf3d::GlslSdfHelperBlock& block, const std::string& functionName)
{
    for (size_t i = 0; i < block.helpers.size(); ++i) {
        if (block.helpers[i].functionName == functionName) {
            return i;
        }
    }

    return block.helpers.size();
}

void expect(bool condition, const std::string& testName, const std::string& message, std::vector<TestFailure>& failures)
{
    if (!condition) {
        failures.push_back({testName, message});
    }
}

void testSdfHelperOrder(std::vector<TestFailure>& failures)
{
    const std::string testName = "sdf helper order";
    sdf3d::GlslEmitter emitter;
    sdf3d::SdfCompileResult result;
    sdf3d::SdfNodePtr sphere = sdf3d::makeSphereNode();
    sphere->stableId = 10;
    sdf3d::SdfNodePtr box = sdf3d::makeBoxNode();
    box->stableId = 20;
    sdf3d::SdfNodePtr unionNode = sdf3d::makeUnionNode({sphere, box});
    unionNode->stableId = 30;

    const sdf3d::GlslSdfHelperBlock block = emitter.emitSdfHelpers(unionNode, result);

    expect(result.errors.empty(), testName, "Expected no helper errors.", failures);
    expect(block.helpers.size() == 3, testName, "Expected one helper per node.", failures);
    expect(findHelper(block, "sdf_node_10") == 0, testName, "Expected sphere helper first.", failures);
    expect(findHelper(block, "sdf_node_20") == 1, testName, "Expected box helper second.", failures);
    expect(findHelper(block, "sdf_node_30") == 2, testName, "Expected union helper last.", failures);
    expect(block.rootFunctionName == "sdf_node_30", testName, "Expected root helper name.", failures);
    if (block.helpers.size() == 3) {
        expect(contains(block.helpers[2].glsl, "return min(sdf_node_10(p), sdf_node_20(p));"), testName, "Expected union to call child helpers.", failures);
    }
}

void testMaterialOverrideSdfHelper(std::vector<TestFailure>& failures)
{
    const std::string testName = "material override sdf helper";
    sdf3d::GlslEmitter emitter;
    sdf3d::SdfCompileResult result;
    sdf3d::SdfNodePtr sphere = sdf3d::makeSphereNode();
    sphere->stableId = 5;
    sdf3d::SdfNodePtr material = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Red");
    material->stableId = 6;
    material->children.push_back(sphere);

    const sdf3d::GlslSdfHelperBlock block = emitter.emitSdfHelpers(material, result);

    expect(result.errors.empty(), testName, "Expected no helper errors.", failures);
    expect(result.materials.empty(), testName, "Expected geometry helpers not to emit materials.", failures);
    expect(block.helpers.size() == 2, testName, "Expected child and material helper.", failures);
    expect(findHelper(block, "sdf_node_5") == 0, testName, "Expected child helper before material helper.", failures);
    expect(findHelper(block, "sdf_node_6") == 1, testName, "Expected material helper last.", failures);
    if (block.helpers.size() == 2) {
        expect(contains(block.helpers[1].glsl, "return sdf_node_5(p);"), testName, "Expected material helper to pass through geometry.", failures);
    }
}

void testTransformSdfHelperUsesRuntimeNodeParam(std::vector<TestFailure>& failures)
{
    const std::string testName = "transform sdf helper uses runtime node param";
    sdf3d::GlslEmitter emitter;
    sdf3d::SdfCompileResult result;
    sdf3d::SdfNodePtr sphere = sdf3d::makeSphereNode();
    sphere->stableId = 10;
    sdf3d::SdfNodePtr translate = sdf3d::makeTranslateNode(sphere, {1.0f, 2.0f, 3.0f}, "Translate");
    translate->stableId = 20;
    result.nodeParams.push_back({20, 0, {1.0f, 2.0f, 3.0f, 0.0f}});

    const sdf3d::GlslSdfHelperBlock block = emitter.emitSdfHelpers(translate, result);

    expect(result.errors.empty(), testName, "Expected no helper errors.", failures);
    expect(block.helpers.size() == 2, testName, "Expected child and translate helper.", failures);
    if (block.helpers.size() == 2) {
        expect(contains(block.helpers[1].glsl, "uNodeParams[0].data0.xyz"), testName, "Expected translate helper to read direct runtime node param slot.", failures);
    }
}

void testSdfHelpersReturnFloatGeometryOnly(std::vector<TestFailure>& failures)
{
    const std::string testName = "sdf helpers return float geometry only";
    sdf3d::GlslEmitter emitter;
    sdf3d::SdfCompileResult result;
    sdf3d::SdfNodePtr sphere = sdf3d::makeSphereNode();
    sphere->stableId = 10;
    sdf3d::SdfNodePtr box = sdf3d::makeBoxNode();
    box->stableId = 20;
    sdf3d::SdfNodePtr unionNode = sdf3d::makeUnionNode({sphere, box});
    unionNode->stableId = 30;
    sdf3d::SdfNodePtr material = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Material");
    material->stableId = 40;
    material->children.push_back(unionNode);

    const sdf3d::GlslSdfHelperBlock block = emitter.emitSdfHelpers(material, result);

    expect(result.errors.empty(), testName, "Expected no helper errors.", failures);
    expect(result.materials.empty(), testName, "Expected SDF helpers to avoid material emission.", failures);
    for (const sdf3d::GlslSdfHelper& helper : block.helpers) {
        expect(contains(helper.glsl, "float " + helper.functionName + "(vec3 p)"), testName, "Expected helper to return float.", failures);
        expect(!contains(helper.glsl, "return vec2("), testName, "Expected helper to return distance only, not vec2 hit data.", failures);
    }
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testSdfHelperOrder(failures);
    testMaterialOverrideSdfHelper(failures);
    testTransformSdfHelperUsesRuntimeNodeParam(failures);
    testSdfHelpersReturnFloatGeometryOnly(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All GlslEmitter tests passed.\n";
    return 0;
}
