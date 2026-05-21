#version 460 core

out vec4 outColor;

uniform vec2 uResolution;
uniform vec3 uCameraPosition;
uniform vec3 uCameraTarget;
uniform vec3 uCameraUp;
uniform float uFovDegrees;
uniform int uMaterialCount;
uniform int uRenderQuality;
uniform int uPathTraceMaxBounces;
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
const int MAX_PATH_BOUNCES = 6;
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

float sdf3d_hash13(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float sdf3d_valueNoise3d(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float x00 = mix(sdf3d_hash13(i + vec3(0.0, 0.0, 0.0)), sdf3d_hash13(i + vec3(1.0, 0.0, 0.0)), f.x);
    float x10 = mix(sdf3d_hash13(i + vec3(0.0, 1.0, 0.0)), sdf3d_hash13(i + vec3(1.0, 1.0, 0.0)), f.x);
    float x01 = mix(sdf3d_hash13(i + vec3(0.0, 0.0, 1.0)), sdf3d_hash13(i + vec3(1.0, 0.0, 1.0)), f.x);
    float x11 = mix(sdf3d_hash13(i + vec3(0.0, 1.0, 1.0)), sdf3d_hash13(i + vec3(1.0, 1.0, 1.0)), f.x);
    float y0 = mix(x00, x10, f.y);
    float y1 = mix(x01, x11, f.y);
    return mix(y0, y1, f.z);
}

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
    int materialType = int(gpuMaterial.metallicEmissionType.z + 0.5);
    if (materialType == 1) {
        float scale = max(gpuMaterial.secondaryAlbedoScale.w, 0.0001);
        vec3 cell = floor(p * scale);
        float checker = mod(cell.x + cell.y + cell.z, 2.0);
        material.albedo = mix(gpuMaterial.albedoRoughness.rgb, gpuMaterial.secondaryAlbedoScale.rgb, checker);
    } else if (materialType == 2) {
        float scale = max(gpuMaterial.secondaryAlbedoScale.w, 0.0001);
        float noise = sdf3d_valueNoise3d(p * scale);
        material.albedo = mix(gpuMaterial.albedoRoughness.rgb, gpuMaterial.secondaryAlbedoScale.rgb, noise);
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

vec3 clampRadiance(vec3 color)
{
    float luminance = max(max(color.r, color.g), color.b);
    if (luminance <= 12.0) {
        return color;
    }
    return color * (12.0 / luminance);
}

vec3 temporalClamp(vec3 currentSample, vec3 historyColor, float sampleCount)
{
    if (sampleCount <= 4.0) {
        return currentSample;
    }

    vec3 tolerance = max(vec3(0.08), abs(historyColor) * 0.45);
    return clamp(currentSample, historyColor - tolerance, historyColor + tolerance);
}

vec3 spatialFilterHistory(vec2 uv, float sampleCount)
{
    vec3 center = texture(uPathTraceAccumulation, uv).rgb;
    if (sampleCount <= 8.0) {
        return center;
    }

    vec2 texel = 1.0 / uResolution;
    vec3 sum = center * 4.0;
    sum += texture(uPathTraceAccumulation, uv + vec2(texel.x, 0.0)).rgb;
    sum += texture(uPathTraceAccumulation, uv - vec2(texel.x, 0.0)).rgb;
    sum += texture(uPathTraceAccumulation, uv + vec2(0.0, texel.y)).rgb;
    sum += texture(uPathTraceAccumulation, uv - vec2(0.0, texel.y)).rgb;
    vec3 crossBlur = sum * 0.125;
    return mix(center, crossBlur, 0.18);
}

uint hashState(uvec2 pixel, uint sampleIndex)
{
    uint state = pixel.x * 1973u + pixel.y * 9277u + sampleIndex * 26699u + 0x9E3779B9u;
    state ^= state >> 16u;
    state *= 2246822519u;
    state ^= state >> 13u;
    state *= 3266489917u;
    state ^= state >> 16u;
    return state;
}

float random01(inout uint state)
{
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return float(state & 0x00FFFFFFu) / float(0x01000000u);
}

vec3 cosineHemisphere(vec3 normal, inout uint rng)
{
    float r1 = random01(rng);
    float r2 = random01(rng);
    float phi = 2.0 * PI * r1;
    float radius = sqrt(r2);
    vec3 local = vec3(cos(phi) * radius, sin(phi) * radius, sqrt(max(0.0, 1.0 - r2)));

    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    return normalize(tangent * local.x + bitangent * local.y + normal * local.z);
}

vec3 powerCosineHemisphere(vec3 axis, float exponent, inout uint rng)
{
    float r1 = random01(rng);
    float r2 = random01(rng);
    float phi = 2.0 * PI * r1;
    float cosTheta = pow(1.0 - r2, 1.0 / (exponent + 1.0));
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    vec3 local = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    vec3 up = abs(axis.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, axis));
    vec3 bitangent = cross(axis, tangent);
    return normalize(tangent * local.x + bitangent * local.y + axis * local.z);
}

vec3 sampleGlossyReflection(vec3 incomingDirection, vec3 normal, float roughness, inout uint rng)
{
    vec3 reflected = reflect(incomingDirection, normal);
    float gloss = max(0.02, 1.0 - roughness);
    float exponent = mix(4.0, 256.0, gloss * gloss);
    vec3 sampled = powerCosineHemisphere(reflected, exponent, rng);
    if (dot(sampled, normal) <= 0.0) {
        sampled = normalize(reflected + normal * (0.25 + roughness));
    }
    return normalize(sampled);
}

bool lightVisible(vec3 origin, vec3 direction, float maxDistance)
{
    float traveled = SURFACE_EPSILON * 4.0;
    for (int i = 0; i < MAX_STEPS; ++i) {
        if (traveled >= maxDistance) {
            return true;
        }
        float distanceToScene = sceneSDF(origin + direction * traveled);
        if (distanceToScene < SURFACE_EPSILON) {
            return false;
        }
        traveled += max(distanceToScene, SURFACE_EPSILON);
    }
    return true;
}

vec3 directLight(vec3 hitPosition, vec3 normal, vec3 viewDirection, SdfMaterialSample material)
{
    vec3 lightDirection = normalize(vec3(-0.4, 0.7, 0.5));
    float nDotL = max(dot(normal, lightDirection), 0.0);
    if (nDotL <= 0.0 || !lightVisible(hitPosition + normal * SURFACE_EPSILON * 4.0, lightDirection, MAX_DISTANCE)) {
        return vec3(0.0);
    }

    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.0);
    vec3 diffuse = material.albedo * nDotL * vec3(1.15, 1.10, 1.0);
    vec3 specular = mix(vec3(0.04), material.albedo, material.metallic) * rim * (1.0 - material.roughness);
    return diffuse + specular;
}

vec3 tracePath(vec3 rayOrigin, vec3 rayDirection, inout uint rng)
{
    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);
    int maxBounces = clamp(uPathTraceMaxBounces, 1, MAX_PATH_BOUNCES);

    for (int bounce = 0; bounce < MAX_PATH_BOUNCES; ++bounce) {
        if (bounce >= maxBounces) {
            break;
        }

        vec3 hitPosition = vec3(0.0);
        float hitDistance = raymarch(rayOrigin, rayDirection, hitPosition);
        if (hitDistance < 0.0) {
            radiance += throughput * backgroundColor(rayDirection);
            break;
        }

        vec3 normal = estimateNormal(hitPosition);
        if (dot(normal, rayDirection) > 0.0) {
            normal = -normal;
        }

        SdfMaterialSample material = sceneMaterial(hitPosition);
        vec3 viewDirection = normalize(-rayDirection);
        radiance += throughput * material.albedo * material.emission;
        radiance += throughput * directLight(hitPosition, normal, viewDirection, material);

        bool sampleMetal = random01(rng) < material.metallic;
        if (sampleMetal) {
            rayDirection = sampleGlossyReflection(rayDirection, normal, material.roughness, rng);
            throughput *= mix(vec3(0.04), material.albedo, material.metallic);
        } else {
            rayDirection = cosineHemisphere(normal, rng);
            throughput *= material.albedo;
        }

        rayOrigin = hitPosition + normal * SURFACE_EPSILON * 4.0;
        if (bounce >= 2) {
            float keep = clamp(max(max(throughput.r, throughput.g), throughput.b), 0.05, 0.95);
            if (random01(rng) > keep) {
                break;
            }
            throughput /= keep;
        }
    }

    return radiance;
}

void main()
{
    vec3 rayOrigin = uCameraPosition;
    uint rng = hashState(uvec2(gl_FragCoord.xy), uPathTraceSampleIndex);
    vec2 jitter = vec2(random01(rng), random01(rng)) - vec2(0.5);
    vec3 rayDirection = rayDirectionFromCamera(gl_FragCoord.xy + jitter);
    vec3 currentSample = clampRadiance(tracePath(rayOrigin, rayDirection, rng));

    vec2 uv = gl_FragCoord.xy / uResolution;
    float sampleCount = float(uPathTraceSampleIndex);
    vec3 previous = spatialFilterHistory(uv, sampleCount);
    currentSample = temporalClamp(currentSample, previous, sampleCount);
    vec3 color = sampleCount <= 0.0 ? currentSample : mix(previous, currentSample, 1.0 / (sampleCount + 1.0));
    outColor = vec4(color, 1.0);
}
