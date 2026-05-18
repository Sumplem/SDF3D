#include "sdf3d/renderer/UniformUploader.h"

#include <glad/gl.h>

namespace sdf3d {
namespace {

constexpr GLuint MATERIAL_BUFFER_BINDING = 0;
constexpr GLuint NODE_PARAM_BUFFER_BINDING = 1;

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
    if (m_nodeParamBuffer == 0) {
        glGenBuffers(1, &m_nodeParamBuffer);
    }
}

void UniformUploader::shutdown()
{
    if (m_materialBuffer != 0) {
        const GLuint buffer = m_materialBuffer;
        glDeleteBuffers(1, &buffer);
        m_materialBuffer = 0;
    }
    if (m_nodeParamBuffer != 0) {
        const GLuint buffer = m_nodeParamBuffer;
        glDeleteBuffers(1, &buffer);
        m_nodeParamBuffer = 0;
    }
}

void UniformUploader::upload(
    unsigned int program,
    int width,
    int height,
    const RenderCamera& camera,
    const RenderGizmo& gizmo,
    RenderQuality quality,
    const std::vector<SdfCompiledMaterial>& materials,
    const std::vector<SdfCompiledNodeParam>& nodeParams)
{
    glUniform2f(glGetUniformLocation(program, "uResolution"), static_cast<float>(width), static_cast<float>(height));
    glUniform3fv(glGetUniformLocation(program, "uCameraPosition"), 1, &camera.position.x);
    glUniform3fv(glGetUniformLocation(program, "uCameraTarget"), 1, &camera.target.x);
    glUniform3fv(glGetUniformLocation(program, "uCameraUp"), 1, &camera.up.x);
    glUniform1f(glGetUniformLocation(program, "uFovDegrees"), camera.fovDegrees);
    glUniform1i(glGetUniformLocation(program, "uGizmoVisible"), gizmo.visible ? 1 : 0);
    glUniform3fv(glGetUniformLocation(program, "uGizmoCenter"), 1, &gizmo.center.x);
    glUniformMatrix3fv(glGetUniformLocation(program, "uGizmoOrientation"), 1, GL_FALSE, &gizmo.orientation[0][0]);
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
    glUniform1i(glGetUniformLocation(program, "uNodeParamCount"), static_cast<GLint>(nodeParams.size()));

    if (m_materialBuffer == 0 || m_nodeParamBuffer == 0) {
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

    const std::vector<GpuNodeParam> packedNodeParams = packNodeParams(nodeParams);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_nodeParamBuffer);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<GLsizeiptr>(packedNodeParams.size() * sizeof(GpuNodeParam)),
        packedNodeParams.empty() ? nullptr : packedNodeParams.data(),
        GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, NODE_PARAM_BUFFER_BINDING, m_nodeParamBuffer);
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

std::vector<UniformUploader::GpuNodeParam> UniformUploader::packNodeParams(const std::vector<SdfCompiledNodeParam>& nodeParams)
{
    std::vector<GpuNodeParam> packed;
    packed.reserve(nodeParams.size());
    for (const SdfCompiledNodeParam& nodeParam : nodeParams) {
        packed.push_back({
            {static_cast<uint32_t>(nodeParam.nodeId & 0xffffffffu), static_cast<uint32_t>(nodeParam.nodeId >> 32u), 0u, 0u},
            {nodeParam.data0[0], nodeParam.data0[1], nodeParam.data0[2], nodeParam.data0[3]},
        });
    }
    return packed;
}

} // namespace sdf3d
