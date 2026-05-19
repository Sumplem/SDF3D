#version 460 core

out vec4 outColor;

uniform vec2 uResolution;
uniform vec3 uCameraPosition;
uniform vec3 uCameraTarget;
uniform vec3 uCameraUp;
uniform float uFovDegrees;
uniform int uMaterialCount;
uniform uint uPathTraceSampleIndex;
uniform sampler2D uPathTraceAccumulation;

struct GpuMaterial {
    vec4 albedoRoughness;
    vec4 metallicEmissionType;
    vec4 secondaryAlbedoScale;
};

layout(std430, binding = 0) readonly buffer MaterialBuffer {
    GpuMaterial uMaterials[];
};

const int MAX_STEPS = 128;
const float MAX_DISTANCE = 100.0;
const float SURFACE_EPSILON = 0.001;
const float NORMAL_EPSILON = 0.00035;
const float PI = 3.14159265358979323846;

struct SdfMaterialSample {
    vec3 albedo;
    float roughness;
    float metallic;
    float emission;
};

SdfMaterialSample sampleMaterial(int materialId, vec3 p)
{
    SdfMaterialSample material;
    if (materialId < 0 || materialId >= uMaterialCount) {
        material.albedo = vec3(0.78, 0.82, 0.88);
        material.roughness = 0.5;
        material.metallic = 0.0;
        material.emission = 0.0;
        return material;
    }

    GpuMaterial gpuMaterial = uMaterials[materialId];
    material.albedo = gpuMaterial.albedoRoughness.rgb;
    if (int(gpuMaterial.metallicEmissionType.z + 0.5) == 1) {
        float scale = max(gpuMaterial.secondaryAlbedoScale.w, 0.0001);
        vec3 cell = floor(p * scale);
        float checker = mod(cell.x + cell.y + cell.z, 2.0);
        material.albedo = mix(gpuMaterial.albedoRoughness.rgb, gpuMaterial.secondaryAlbedoScale.rgb, checker);
    }
    material.roughness = clamp(gpuMaterial.albedoRoughness.a, 0.02, 1.0);
    material.metallic = clamp(gpuMaterial.metallicEmissionType.x, 0.0, 1.0);
    material.emission = gpuMaterial.metallicEmissionType.y;
    return material;
}

SdfMaterialSample selectMaterial(bool useFirst, SdfMaterialSample first, SdfMaterialSample second)
{
    return useFirst ? first : second;
}

SdfMaterialSample mixMaterial(SdfMaterialSample first, SdfMaterialSample second, float t)
{
    SdfMaterialSample material;
    material.albedo = mix(first.albedo, second.albedo, t);
    material.roughness = mix(first.roughness, second.roughness, t);
    material.metallic = mix(first.metallic, second.metallic, t);
    material.emission = mix(first.emission, second.emission, t);
    return material;
}

// SDF3D_SCENE_BEGIN
float sceneSDF(vec3 p)
{
    return length(p) - 1.0;
}

SdfMaterialSample sceneMaterial(vec3 p)
{
    return sampleMaterial(0, p);
}

float sceneNodeSDF(int nodeId, vec3 p)
{
    return 1e6;
}
// SDF3D_SCENE_END

vec3 estimateNormal(vec3 p)
{
    vec2 e = vec2(NORMAL_EPSILON, 0.0);
    return normalize(vec3(
        sceneSDF(p + e.xyy) - sceneSDF(p - e.xyy),
        sceneSDF(p + e.yxy) - sceneSDF(p - e.yxy),
        sceneSDF(p + e.yyx) - sceneSDF(p - e.yyx)
    ));
}

float raymarch(vec3 rayOrigin, vec3 rayDirection, out vec3 hitPosition)
{
    float distanceTraveled = 0.0;
    for (int i = 0; i < MAX_STEPS; ++i) {
        hitPosition = rayOrigin + rayDirection * distanceTraveled;
        float distanceToScene = sceneSDF(hitPosition);
        if (distanceToScene < SURFACE_EPSILON) {
            return distanceTraveled;
        }
        distanceTraveled += distanceToScene;
        if (distanceTraveled > MAX_DISTANCE) {
            break;
        }
    }
    return -1.0;
}

vec3 backgroundColor(vec3 rayDirection)
{
    return mix(vec3(0.05, 0.06, 0.07), vec3(0.12, 0.14, 0.17), max(rayDirection.y, 0.0));
}

vec3 rayDirectionFromCamera(vec2 fragCoord)
{
    vec2 uv = (fragCoord * 2.0 - uResolution) / uResolution.y;
    float focalLength = 1.0 / tan(radians(uFovDegrees) * 0.5);

    vec3 forward = normalize(uCameraTarget - uCameraPosition);
    vec3 right = normalize(cross(forward, uCameraUp));
    vec3 up = normalize(cross(right, forward));
    return normalize(uv.x * right + uv.y * up + focalLength * forward);
}

vec3 directPathTraceStub(vec3 rayOrigin, vec3 rayDirection)
{
    vec3 hitPosition = vec3(0.0);
    float hitDistance = raymarch(rayOrigin, rayDirection, hitPosition);
    if (hitDistance < 0.0) {
        return backgroundColor(rayDirection);
    }

    vec3 normal = estimateNormal(hitPosition);
    vec3 lightDirection = normalize(vec3(-0.4, 0.7, 0.5));
    vec3 viewDirection = normalize(rayOrigin - hitPosition);
    SdfMaterialSample material = sceneMaterial(hitPosition);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.0);
    vec3 ambient = material.albedo * 0.12;
    vec3 direct = material.albedo * nDotL * vec3(1.15, 1.10, 1.0);
    vec3 specular = mix(vec3(0.04), material.albedo, material.metallic) * rim * (1.0 - material.roughness);
    return ambient + direct + specular + material.albedo * material.emission;
}

void main()
{
    vec3 rayOrigin = uCameraPosition;
    vec3 rayDirection = rayDirectionFromCamera(gl_FragCoord.xy);
    vec3 currentSample = directPathTraceStub(rayOrigin, rayDirection);

    vec3 previous = texture(uPathTraceAccumulation, gl_FragCoord.xy / uResolution).rgb;
    float sampleCount = float(uPathTraceSampleIndex);
    vec3 color = sampleCount <= 0.0 ? currentSample : mix(previous, currentSample, 1.0 / (sampleCount + 1.0));
    outColor = vec4(color, 1.0);
}
