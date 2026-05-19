#include "sdf3d/scene/SdfCompiler.h"
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
    expect(contains(result.glsl, "SdfMaterialSample sceneMaterial(vec3 p)"), testName, "Expected sceneMaterial declaration.", failures);
    expect(!contains(result.glsl, "sceneSDFWithMaterial"), testName, "Expected legacy material-aware sceneSDF removed.", failures);
    expect(contains(result.glsl, "length(p) - 1.000000"), testName, "Expected unit sphere expression.", failures);
    expect(!result.usesBox, testName, "Expected box helper to be unused.", failures);
    expect(!result.usesCylinder, testName, "Expected cylinder helper to be unused.", failures);
    expect(!result.usesSmoothMin, testName, "Expected smooth-min helper to be unused.", failures);
    expect(!result.usesRotate, testName, "Expected rotation helper to be unused.", failures);
    expect(!contains(result.glsl, "sdf3d_box"), testName, "Expected box helper to be omitted.", failures);
    expect(!contains(result.glsl, "sdf3d_cylinder"), testName, "Expected cylinder helper to be omitted.", failures);
    expect(!contains(result.glsl, "sdf3d_smin"), testName, "Expected smooth-min helper to be omitted.", failures);
    expect(!contains(result.glsl, "sdf3d_rotationQuat"), testName, "Expected rotation helper to be omitted.", failures);
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
    expect(contains(result.glsl, "p / vec3(2.000000, 2.000000, 2.000000)"), testName, "Expected scaled point expression.", failures);
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
        sdf3d::makeSmoothUnionNode({sdf3d::makeSphereNode("A"), sdf3d::makeBoxNode({1.0f, 1.0f, 1.0f}, "B")}, 0.4f)
    );

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.usesSmoothMin, testName, "Expected smooth-min helper flag.", failures);
    expect(contains(result.glsl, "float sdf3d_smin"), testName, "Expected smooth-min helper.", failures);
    expect(contains(result.glsl, "sdf3d_smin("), testName, "Expected smooth union call.", failures);
    expect(contains(result.glsl, "0.400000"), testName, "Expected smoothness parameter.", failures);
    expect(contains(result.glsl, "mixMaterial("), testName, "Expected smooth union material blending.", failures);
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
    expect(contains(result.glsl, "mixMaterial("), testName, "Expected smooth subtract material blending.", failures);
}

void testSmoothIntersect(std::vector<TestFailure>& failures)
{
    const std::string testName = "smooth intersect";
    const sdf3d::SdfCompiler compiler;
    const sdf3d::SdfCompileResult result = compiler.compile(
        sdf3d::makeSmoothIntersectNode({sdf3d::makeSphereNode("A"), sdf3d::makeBoxNode({1.0f, 1.0f, 1.0f}, "B")}, 0.3f)
    );

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.usesSmoothMin, testName, "Expected smooth-min helper flag.", failures);
    expect(contains(result.glsl, "-sdf3d_smin"), testName, "Expected smooth intersect expression.", failures);
    expect(contains(result.glsl, "0.300000"), testName, "Expected smoothness parameter.", failures);
    expect(contains(result.glsl, "mixMaterial("), testName, "Expected smooth intersect material blending.", failures);
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
    expect(contains(result.glsl, "mat3 sdf3d_rotationQuat"), testName, "Expected rotation helper.", failures);
    expect(!contains(result.glsl, "normalize(q)"), testName, "Expected quaternion normalize to stay CPU-side.", failures);
    expect(contains(result.glsl, "float xy = x * y;"), testName, "Expected precomputed quaternion products.", failures);
    expect(contains(result.glsl, "transpose(sdf3d_rotationQuat(vec4("), testName, "Expected inverse quaternion rotation expression.", failures);
}

void testMaterialMetadata(std::vector<TestFailure>& failures)
{
    const std::string testName = "material metadata";
    const sdf3d::SdfCompiler compiler;

    sdf3d::SdfNodePtr redSphere = sdf3d::makeSphereNode("Red Sphere");
    sdf3d::SdfNodePtr redMaterial = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Red");
    redMaterial->material.albedo = {1.0f, 0.0f, 0.0f};
    redMaterial->material.emission = 0.5f;
    redMaterial->children.push_back(redSphere);

    sdf3d::SdfNodePtr blueBox = sdf3d::makeBoxNode({1.0f, 1.0f, 1.0f}, "Blue Box");
    sdf3d::SdfNodePtr blueMaterial = sdf3d::makeSdfNode(sdf3d::SdfNodeType::MaterialOverride, "Blue");
    blueMaterial->material.albedo = {0.0f, 0.0f, 1.0f};
    blueMaterial->material.roughness = 0.25f;
    blueMaterial->children.push_back(blueBox);

    const sdf3d::SdfCompileResult result = compiler.compile(sdf3d::makeUnionNode({redMaterial, blueMaterial}));

    expect(result.errors.empty(), testName, "Expected no compiler errors.", failures);
    expect(result.materials.size() == 3, testName, "Expected default plus material override nodes.", failures);
    if (result.materials.size() == 3) {
        expect(result.materials[1].material.albedo.x == 1.0f, testName, "Expected first override material to be red.", failures);
        expect(result.materials[1].material.emission == 0.5f, testName, "Expected first override emission to be preserved.", failures);
        expect(result.materials[2].material.albedo.z == 1.0f, testName, "Expected second override material to be blue.", failures);
        expect(result.materials[2].material.roughness == 0.25f, testName, "Expected second override roughness to be preserved.", failures);
    }
    expect(contains(result.glsl, "SdfMaterialSample sceneMaterial(vec3 p)"), testName, "Expected deferred material entry point.", failures);
    expect(contains(result.glsl, "selectMaterial("), testName, "Expected material choice between override samples.", failures);
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

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All SDF node tests passed.\n";
    return 0;
}
