#include "sdf3d/renderer/UniformUploader.h"

#include <string>

#include <glad/gl.h>

namespace sdf3d {
namespace {

constexpr size_t MAX_SHADER_MATERIALS = 64;

void setUniformVec3(GLuint program, const std::string& name, const glm::vec3& value)
{
    const GLint location = glGetUniformLocation(program, name.c_str());
    if (location >= 0) {
        glUniform3fv(location, 1, &value.x);
    }
}

void setUniformFloat(GLuint program, const std::string& name, float value)
{
    const GLint location = glGetUniformLocation(program, name.c_str());
    if (location >= 0) {
        glUniform1f(location, value);
    }
}

} // namespace

void UniformUploader::upload(unsigned int program, int width, int height, const RenderCamera& camera, const std::vector<SdfCompiledMaterial>& materials) const
{
    glUniform2f(glGetUniformLocation(program, "uResolution"), static_cast<float>(width), static_cast<float>(height));
    glUniform3fv(glGetUniformLocation(program, "uCameraPosition"), 1, &camera.position.x);
    glUniform3fv(glGetUniformLocation(program, "uCameraTarget"), 1, &camera.target.x);
    glUniform3fv(glGetUniformLocation(program, "uCameraUp"), 1, &camera.up.x);
    glUniform1f(glGetUniformLocation(program, "uFovDegrees"), camera.fovDegrees);

    const size_t materialCount = materialCountForShader(materials.size());
    glUniform1i(glGetUniformLocation(program, "uMaterialCount"), static_cast<GLint>(materialCount));
    for (size_t i = 0; i < materialCount; ++i) {
        const SdfMaterial& material = materials[i].material;
        const std::string index = std::to_string(i);
        // AGENT: Uniform arrays keep first material path simple and avoid a
        // renderer-side UBO layout contract until material count becomes large.
        setUniformVec3(program, "uMaterialAlbedo[" + index + "]", material.albedo);
        setUniformFloat(program, "uMaterialRoughness[" + index + "]", material.roughness);
        setUniformFloat(program, "uMaterialMetallic[" + index + "]", material.metallic);
        setUniformFloat(program, "uMaterialEmission[" + index + "]", material.emission);
    }
}

size_t UniformUploader::materialCountForShader(size_t materialCount)
{
    return materialCount < MAX_SHADER_MATERIALS ? materialCount : MAX_SHADER_MATERIALS;
}

} // namespace sdf3d
