#include "sdf3d/renderer/UniformUploader.h"

#include <cstddef>
#include <cstdint>
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

void testMaterialCountForShader(std::vector<TestFailure>& failures)
{
    const std::string testName = "material count for shader";

    expect(sdf3d::UniformUploader::materialCountForShader(0) == 0, testName, "Expected zero materials.", failures);
    expect(sdf3d::UniformUploader::materialCountForShader(63) == 63, testName, "Expected under-cap count preserved.", failures);
    expect(sdf3d::UniformUploader::materialCountForShader(65) == 65, testName, "Expected SSBO path not to clamp at old uniform cap.", failures);
}

void testPackedMaterialLayout(std::vector<TestFailure>& failures)
{
    const std::string testName = "packed material layout";
    std::vector<sdf3d::SdfCompiledMaterial> materials(2);
    materials[1].material.albedo = {0.25f, 0.5f, 0.75f};
    materials[1].material.secondaryAlbedo = {0.1f, 0.2f, 0.3f};
    materials[1].material.roughness = 0.35f;
    materials[1].material.metallic = 0.6f;
    materials[1].material.emission = 1.25f;
    materials[1].material.type = sdf3d::SdfMaterialType::Checker;
    materials[1].material.patternScale = 7.0f;

    const std::vector<sdf3d::UniformUploader::GpuMaterial> packed = sdf3d::UniformUploader::packMaterials(materials);

    expect(sizeof(sdf3d::UniformUploader::GpuMaterial) == sizeof(float) * 12, testName, "Expected three vec4 material layout.", failures);
    expect(packed.size() == 2, testName, "Expected all materials packed.", failures);
    if (packed.size() == 2) {
        expect(packed[1].albedoRoughness.x == 0.25f, testName, "Expected albedo x packed.", failures);
        expect(packed[1].albedoRoughness.w == 0.35f, testName, "Expected roughness packed.", failures);
        expect(packed[1].metallicEmissionType.x == 0.6f, testName, "Expected metallic packed.", failures);
        expect(packed[1].metallicEmissionType.y == 1.25f, testName, "Expected emission packed.", failures);
        expect(packed[1].metallicEmissionType.z == 1.0f, testName, "Expected material type packed.", failures);
        expect(packed[1].secondaryAlbedoScale.z == 0.3f, testName, "Expected secondary albedo packed.", failures);
        expect(packed[1].secondaryAlbedoScale.w == 7.0f, testName, "Expected pattern scale packed.", failures);
    }
}

void testPackedNodeParamLayout(std::vector<TestFailure>& failures)
{
    const std::string testName = "packed node param layout";
    std::vector<sdf3d::SdfCompiledNodeParam> params(1);
    params[0].nodeId = (static_cast<uint64_t>(2) << 32u) | 7u;
    params[0].data0 = {1.0f, 2.0f, 3.0f, 4.0f};

    const std::vector<sdf3d::UniformUploader::GpuNodeParam> packed = sdf3d::UniformUploader::packNodeParams(params);

    expect(sizeof(sdf3d::UniformUploader::GpuNodeParam) == sizeof(float) * 8, testName, "Expected two vec4 node param layout.", failures);
    expect(packed.size() == 1, testName, "Expected one node param packed.", failures);
    if (packed.size() == 1) {
        expect(packed[0].id.x == 7u, testName, "Expected low node id packed.", failures);
        expect(packed[0].id.y == 2u, testName, "Expected high node id packed.", failures);
        expect(packed[0].data0.z == 3.0f, testName, "Expected data vec packed.", failures);
    }
}

void testRenderGizmoDefaults(std::vector<TestFailure>& failures)
{
    const std::string testName = "render gizmo defaults";
    const sdf3d::RenderGizmo gizmo;

    expect(!gizmo.visible, testName, "Expected hidden gizmo by default.", failures);
    expect(gizmo.activeAxis == -1, testName, "Expected no active axis by default.", failures);
    expect(gizmo.hoverAxis == -1, testName, "Expected no hover axis by default.", failures);
    expect(gizmo.type == 0, testName, "Expected translate gizmo type default.", failures);
    expect(gizmo.rotateStyle == sdf3d::GizmoRotateStyle::Rings, testName, "Expected rotate rings style default.", failures);
    expect(gizmo.arrowLength == 1.0f, testName, "Expected default arrow length.", failures);
    expect(gizmo.arrowRadius > 0.0f, testName, "Expected positive arrow radius.", failures);
    expect(gizmo.ringRadius > 0.0f, testName, "Expected positive ring radius.", failures);
    expect(gizmo.tubeRadius > 0.0f, testName, "Expected positive tube radius.", failures);
}

void testRenderQualityValues(std::vector<TestFailure>& failures)
{
    const std::string testName = "render quality values";

    expect(static_cast<int>(sdf3d::RenderQuality::Low) == 0, testName, "Expected low quality uniform value.", failures);
    expect(static_cast<int>(sdf3d::RenderQuality::Medium) == 1, testName, "Expected medium quality uniform value.", failures);
    expect(static_cast<int>(sdf3d::RenderQuality::High) == 2, testName, "Expected high quality uniform value.", failures);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testMaterialCountForShader(failures);
    testPackedMaterialLayout(failures);
    testPackedNodeParamLayout(failures);
    testRenderGizmoDefaults(failures);
    testRenderQualityValues(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All UniformUploader tests passed.\n";
    return 0;
}
