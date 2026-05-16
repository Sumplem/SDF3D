#version 460 core

out vec4 outColor;

uniform vec2 uResolution;
uniform vec3 uCameraPosition;
uniform vec3 uCameraTarget;
uniform vec3 uCameraUp;
uniform float uFovDegrees;
uniform int uMaterialCount;

struct GpuMaterial {
    vec4 albedoRoughness;
    vec4 metallicEmission;
};

layout(std430, binding = 0) readonly buffer MaterialBuffer {
    GpuMaterial uMaterials[];
};

const int MAX_STEPS = 128;
const float MAX_DISTANCE = 100.0;
const float SURFACE_EPSILON = 0.001;
const float NORMAL_EPSILON = 0.00035;
const float PI = 3.14159265358979323846;
const float SHADOW_MIN_DISTANCE = 0.01;
const float SHADOW_MAX_DISTANCE = 32.0;
const float SHADOW_SOFTNESS = 10.0;
const int AO_STEPS = 5;
const float AO_STEP_SIZE = 0.08;
const float AO_STRENGTH = 1.4;

struct SdfMaterialSample {
    vec3 albedo;
    float roughness;
    float metallic;
    float emission;
};

SdfMaterialSample sampleMaterial(int materialId)
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
    material.roughness = clamp(gpuMaterial.albedoRoughness.a, 0.02, 1.0);
    material.metallic = clamp(gpuMaterial.metallicEmission.x, 0.0, 1.0);
    material.emission = gpuMaterial.metallicEmission.y;
    return material;
}

SdfMaterialSample selectMaterial(bool useFirst, SdfMaterialSample first, SdfMaterialSample second)
{
    if (useFirst) {
        return first;
    }

    return second;
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

// AGENT: Renderer replaces the block between these markers with GLSL emitted
// by SdfCompiler when the scene graph changes.
// SDF3D_SCENE_BEGIN
float sceneSDF(vec3 p)
{
    return length(p) - 1.0;
}

SdfMaterialSample sceneMaterial(vec3 p)
{
    return sampleMaterial(0);
}
// SDF3D_SCENE_END

vec3 estimateNormal(vec3 p)
{
    // AGENT: Central differences are cheap and stable enough for the M2
    // hardcoded scene; analytic normals can be introduced per-node later.
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

float softShadow(vec3 rayOrigin, vec3 rayDirection)
{
    float visibility = 1.0;
    float distanceTraveled = SHADOW_MIN_DISTANCE;

    for (int i = 0; i < MAX_STEPS; ++i) {
        if (distanceTraveled >= SHADOW_MAX_DISTANCE) {
            break;
        }

        float distanceToScene = sceneSDF(rayOrigin + rayDirection * distanceTraveled);
        if (distanceToScene < SURFACE_EPSILON) {
            return 0.0;
        }

        visibility = min(visibility, SHADOW_SOFTNESS * distanceToScene / distanceTraveled);
        distanceTraveled += clamp(distanceToScene, 0.02, 0.5);
    }

    return clamp(visibility, 0.0, 1.0);
}

float ambientOcclusion(vec3 position, vec3 normal)
{
    float occlusion = 0.0;
    float weight = 1.0;

    for (int i = 1; i <= AO_STEPS; ++i) {
        float sampleDistance = AO_STEP_SIZE * float(i);
        float sceneDistance = sceneSDF(position + normal * sampleDistance);
        occlusion += (sampleDistance - sceneDistance) * weight;
        weight *= 0.55;
    }

    return clamp(1.0 - occlusion * AO_STRENGTH, 0.0, 1.0);
}

float distributionGGX(vec3 normal, vec3 halfVector, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float nDotH = max(dot(normal, halfVector), 0.0);
    float nDotHSquared = nDotH * nDotH;
    float denominator = nDotHSquared * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / max(PI * denominator * denominator, 0.0001);
}

float geometrySchlickGGX(float nDotDirection, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotDirection / max(nDotDirection * (1.0 - k) + k, 0.0001);
}

float geometrySmith(vec3 normal, vec3 viewDirection, vec3 lightDirection, float roughness)
{
    float nDotV = max(dot(normal, viewDirection), 0.0);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    return geometrySchlickGGX(nDotV, roughness) * geometrySchlickGGX(nDotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 pbrDirectLighting(vec3 normal, vec3 viewDirection, vec3 lightDirection, vec3 albedo, float roughness, float metallic)
{
    vec3 halfVector = normalize(viewDirection + lightDirection);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    float nDotV = max(dot(normal, viewDirection), 0.0);
    float hDotV = max(dot(halfVector, viewDirection), 0.0);

    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 fresnel = fresnelSchlick(hDotV, f0);
    float distribution = distributionGGX(normal, halfVector, roughness);
    float geometry = geometrySmith(normal, viewDirection, lightDirection, roughness);
    vec3 specular = distribution * geometry * fresnel / max(4.0 * nDotV * nDotL, 0.0001);

    vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * albedo / PI;
    vec3 lightColor = vec3(1.15, 1.10, 1.0);
    return (diffuse + specular) * lightColor * nDotL;
}

float gridLine(vec2 p)
{
    vec2 cell = abs(fract(p - 0.5) - 0.5) / fwidth(p);
    return 1.0 - min(min(cell.x, cell.y), 1.0);
}

vec3 backgroundColor(vec3 rayOrigin, vec3 rayDirection)
{
    vec3 color = mix(vec3(0.05, 0.06, 0.07), vec3(0.12, 0.14, 0.17), max(rayDirection.y, 0.0));

    float planeHit = (-1.0 - rayOrigin.y) / rayDirection.y;
    if (planeHit > 0.0) {
        vec3 p = rayOrigin + rayDirection * planeHit;
        float grid = gridLine(p.xz);
        float axisX = smoothstep(0.04, 0.0, abs(p.z));
        float axisZ = smoothstep(0.04, 0.0, abs(p.x));
        vec3 gridColor = vec3(0.22);

        // AGENT: Colored world axes make orbit visible even while the M2 SDF
        // remains a single sphere.
        gridColor = mix(gridColor, vec3(0.75, 0.18, 0.14), axisX);
        gridColor = mix(gridColor, vec3(0.16, 0.36, 0.95), axisZ);

        float fade = exp(-0.04 * length(p.xz));
        color = mix(color, gridColor, grid * fade);
    }

    return color;
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

void main()
{
    vec3 rayOrigin = uCameraPosition;
    vec3 rayDirection = rayDirectionFromCamera(gl_FragCoord.xy);
    vec3 hitPosition = vec3(0.0);

    float hitDistance = raymarch(rayOrigin, rayDirection, hitPosition);
    if (hitDistance < 0.0) {
        outColor = vec4(backgroundColor(rayOrigin, rayDirection), 1.0);
        return;
    }

    vec3 normal = estimateNormal(hitPosition);
    vec3 lightDirection = normalize(vec3(-0.4, 0.7, 0.5));
    vec3 viewDirection = normalize(rayOrigin - hitPosition);
    float shadow = softShadow(hitPosition + normal * SURFACE_EPSILON * 2.0, lightDirection);
    float occlusion = ambientOcclusion(hitPosition, normal);
    SdfMaterialSample material = sceneMaterial(hitPosition);
    vec3 baseColor = material.albedo;
    float roughness = material.roughness;
    float metallic = material.metallic;

    vec3 ambient = baseColor * 0.18 * occlusion * (1.0 - metallic * 0.35);
    vec3 direct = pbrDirectLighting(normal, viewDirection, lightDirection, baseColor, roughness, metallic) * shadow;
    vec3 color = ambient + direct + baseColor * material.emission;

    outColor = vec4(color, 1.0);
}
