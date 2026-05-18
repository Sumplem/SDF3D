#include "sdf3d/renderer/UniformUploader.h"

#include <glad/gl.h>

namespace sdf3d {
namespace {

constexpr GLuint MATERIAL_BUFFER_BINDING = 0;

} // namespace

UniformUploader::~UniformUploader()
{
    shutdown();
}

void UniformUploader::init()
{
    if (m_materialBuffer == 0) {
        glGenBuffers(1, &m_materialBuffer);
    }
}

void UniformUploader::shutdown()
{
    if (m_materialBuffer != 0) {
        const GLuint buffer = m_materialBuffer;
        glDeleteBuffers(1, &buffer);
        m_materialBuffer = 0;
    }
}

void UniformUploader::upload(
    unsigned int program,
    int width,
    int height,
    const RenderCamera& camera,
    const RenderGizmo& gizmo,
    RenderQuality quality,
    const std::vector<SdfCompiledMaterial>& materials)
{
    glUniform2f(glGetUniformLocation(program, "uResolution"), static_cast<float>(width), static_cast<float>(height));
    glUniform3fv(glGetUniformLocation(program, "uCameraPosition"), 1, &camera.position.x);
    glUniform3fv(glGetUniformLocation(program, "uCameraTarget"), 1, &camera.target.x);
    glUniform3fv(glGetUniformLocation(program, "uCameraUp"), 1, &camera.up.x);
    glUniform1f(glGetUniformLocation(program, "uFovDegrees"), camera.fovDegrees);
    glUniform1i(glGetUniformLocation(program, "uGizmoVisible"), gizmo.visible ? 1 : 0);
    glUniform3fv(glGetUniformLocation(program, "uGizmoCenter"), 1, &gizmo.center.x);
    glUniform1f(glGetUniformLocation(program, "uGizmoArrowLength"), gizmo.arrowLength);
    glUniform1f(glGetUniformLocation(program, "uGizmoArrowRadius"), gizmo.arrowRadius);
    glUniform1f(glGetUniformLocation(program, "uGizmoR"), gizmo.ringRadius);
    glUniform1f(glGetUniformLocation(program, "uGizmoTube"), gizmo.tubeRadius);
    glUniform1i(glGetUniformLocation(program, "uGizmoActiveAxis"), gizmo.activeAxis);
    glUniform1i(glGetUniformLocation(program, "uGizmoHoverAxis"), gizmo.hoverAxis);
    glUniform1i(glGetUniformLocation(program, "uGizmoType"), gizmo.type);
    glUniform1i(glGetUniformLocation(program, "uHighlightNodeId"), static_cast<GLint>(gizmo.highlightNodeId));
    glUniform1i(glGetUniformLocation(program, "uRenderQuality"), static_cast<GLint>(quality));

    const size_t materialCount = materialCountForShader(materials.size());
    glUniform1i(glGetUniformLocation(program, "uMaterialCount"), static_cast<GLint>(materialCount));

    if (m_materialBuffer == 0) {
        init();
    }

    const std::vector<GpuMaterial> packedMaterials = packMaterials(materials);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_materialBuffer);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(packedMaterials.size() * sizeof(GpuMaterial)),
        packedMaterials.empty() ? nullptr : packedMaterials.data(),
        GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MATERIAL_BUFFER_BINDING, m_materialBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

size_t UniformUploader::materialCountForShader(size_t materialCount)
{
    return materialCount;
}

std::vector<UniformUploader::GpuMaterial> UniformUploader::packMaterials(const std::vector<SdfCompiledMaterial>& materials)
{
    std::vector<GpuMaterial> packed;
    packed.reserve(materials.size());
    for (const SdfCompiledMaterial& compiledMaterial : materials) {
        const SdfMaterial& material = compiledMaterial.material;
        packed.push_back({
            {material.albedo.x, material.albedo.y, material.albedo.z, material.roughness},
            {material.metallic, material.emission, 0.0f, 0.0f},
        });
    }
    return packed;
}

} // namespace sdf3d
