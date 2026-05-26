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
uniform vec3 uEnvColor;
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
const int RUSSIAN_ROULETTE_START_BOUNCE = 2;
const float RUSSIAN_ROULETTE_MIN_KEEP = 0.05;
const float RUSSIAN_ROULETTE_MAX_KEEP = 0.95;
const vec3 DIRECT_LIGHT_DIRECTION = vec3(-0.421637, 0.737865, 0.527046);
const vec3 DIRECT_LIGHT_RADIANCE = vec3(1.15, 1.10, 1.0);
const float DIRECT_LIGHT_PDF = 1.0;
const float MIN_PDF = 0.0001;
const float MAX_EMISSIVE_RADIANCE = 16.0;
const int EMISSIVE_SURFACE_HIT_SAMPLES = 1;

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
struct SdfNodeParam
{
    vec4 data0;
    vec4 data1;
};

layout(std430, binding = 1) readonly buffer NodeParamBuffer
{
    SdfNodeParam uNodeParams[];
};

uniform int uNodeParamCount;

layout(std430, binding = 2) readonly buffer InstancePositionBuffer
{
    vec4 uInstancePositions[];
};

uniform int uInstancePositionCount;

float sdf3d_sphere_instances(vec3 p, float radius, int first, int count)
{
    float distance = 1e6;
    int last = min(first + count, uInstancePositionCount);
    for (int i = first; i < last; ++i) {
        distance = min(distance, length(p - uInstancePositions[i].xyz) - radius);
    }
    return distance;
}

float sdf3d_instance_sphere(vec3 p, vec4 params, vec4 range)
{
    float distance = 1e6;
    int first = int(range.x);
    int last = min(first + int(range.y), uInstancePositionCount);
    for (int i = first; i < last; ++i) {
        distance = min(distance, length(p - uInstancePositions[i].xyz) - params.x);
    }
    return distance;
}

float sdf3d_instance_box(vec3 p, vec4 params, vec4 range)
{
    float distance = 1e6;
    int first = int(range.x);
    int last = min(first + int(range.y), uInstancePositionCount);
    for (int i = first; i < last; ++i) {
        vec3 q = abs(p - uInstancePositions[i].xyz) - params.xyz;
        distance = min(distance, length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0));
    }
    return distance;
}

float sdf3d_instance_cylinder(vec3 p, vec4 params, vec4 range)
{
    float distance = 1e6;
    int first = int(range.x);
    int last = min(first + int(range.y), uInstancePositionCount);
    for (int i = first; i < last; ++i) {
        vec3 localP = p - uInstancePositions[i].xyz;
        vec2 d = abs(vec2(length(localP.xz), localP.y)) - vec2(params.x, params.y);
        distance = min(distance, min(max(d.x, d.y), 0.0) + length(max(d, 0.0)));
    }
    return distance;
}

float sdf3d_instance_torus(vec3 p, vec4 params, vec4 range)
{
    float distance = 1e6;
    int first = int(range.x);
    int last = min(first + int(range.y), uInstancePositionCount);
    for (int i = first; i < last; ++i) {
        vec3 localP = p - uInstancePositions[i].xyz;
        distance = min(distance, length(vec2(length(localP.xz) - params.x, localP.y)) - params.y);
    }
    return distance;
}

float sdf3d_instance_plane(vec3 p, vec4 params, vec4 range)
{
    float distance = 1e6;
    int first = int(range.x);
    int last = min(first + int(range.y), uInstancePositionCount);
    vec3 normal = normalize(params.xyz);
    for (int i = first; i < last; ++i) {
        distance = min(distance, dot(p - uInstancePositions[i].xyz, normal) + params.w);
    }
    return distance;
}

float sdf3d_instance_capsule(vec3 p, vec4 params, vec4 range)
{
    float distance = 1e6;
    int first = int(range.x);
    int last = min(first + int(range.y), uInstancePositionCount);
    for (int i = first; i < last; ++i) {
        vec3 localP = p - uInstancePositions[i].xyz;
        distance = min(distance, length(vec3(localP.x, localP.y - clamp(localP.y, -params.y, params.y), localP.z)) - params.x);
    }
    return distance;
}

float sdf3d_instance_cone(vec3 p, vec4 params, vec4 range)
{
    float distance = 1e6;
    int first = int(range.x);
    int last = min(first + int(range.y), uInstancePositionCount);
    for (int i = first; i < last; ++i) {
        vec3 localP = p - uInstancePositions[i].xyz;
        vec2 q = vec2(length(localP.xz), localP.y);
        vec2 k1 = vec2(0.0, params.y);
        vec2 k2 = vec2(-params.x, 2.0 * params.y);
        vec2 ca = vec2(q.x - min(q.x, q.y < 0.0 ? params.x : 0.0), abs(q.y) - params.y);
        vec2 cb = q - k1 + k2 * clamp(dot(k1 - q, k2) / dot(k2, k2), 0.0, 1.0);
        float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;
        distance = min(distance, s * sqrt(min(dot(ca, ca), dot(cb, cb))));
    }
    return distance;
}

float sdf3d_instance_round_box(vec3 p, vec4 params, vec4 range)
{
    float distance = 1e6;
    int first = int(range.x);
    int last = min(first + int(range.y), uInstancePositionCount);
    for (int i = first; i < last; ++i) {
        vec3 q = abs(p - uInstancePositions[i].xyz) - params.xyz;
        distance = min(distance, length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - params.w);
    }
    return distance;
}

SdfMaterialSample sdf3d_material_1(vec3 p)
{
    return SdfMaterialSample(mix(vec3(0.852941, 0.196511, 0.196511), vec3(0.058824, 0.055652, 0.055652), clamp(sdf3d_valueNoise3d(p * max(4.220000, 0.0001)), 0.0, 1.0)), clamp(0.500000, 0.02, 1.0), clamp(0.000000, 0.0, 1.0), max(0.000000, 0.0));
}

float sdf_node_2(vec3 p)
{
    return sdf3d_instance_sphere(p, uNodeParams[0].data0, uNodeParams[0].data1);
}

float sdf_node_4(vec3 p)
{
    return sdf3d_instance_sphere(p, uNodeParams[1].data0, uNodeParams[1].data1);
}

float sdf_node_7(vec3 p)
{
    return sdf3d_instance_sphere(p, uNodeParams[2].data0, uNodeParams[2].data1);
}

float sdf_node_9(vec3 p)
{
    return (length(p) - uNodeParams[3].data0.x);
}

float sdf_node_12(vec3 p)
{
    return sdf_node_9(p);
}

float sdf_node_10(vec3 p)
{
    return sdf_node_12((p - uNodeParams[4].data0.xyz));
}

float sdf_node_5(vec3 p)
{
    return min(min(min(sdf_node_2(p), sdf_node_4(p)), sdf_node_7(p)), sdf_node_10(p));
}

vec2 sceneSDFWithId(vec3 p)
{
    vec2 sdf3d_hit0 = vec2(sdf_node_2(p), 2.0);
    vec2 sdf3d_hit1 = vec2(sdf_node_4(p), 4.0);
    vec2 sdf3d_hit2 = vec2(min(sdf3d_hit0.x, sdf3d_hit1.x), (sdf3d_hit0.x < sdf3d_hit1.x ? sdf3d_hit0.y : sdf3d_hit1.y));
    vec2 sdf3d_hit3 = vec2(sdf_node_7(p), 7.0);
    vec2 sdf3d_hit4 = vec2(min(sdf3d_hit2.x, sdf3d_hit3.x), (sdf3d_hit2.x < sdf3d_hit3.x ? sdf3d_hit2.y : sdf3d_hit3.y));
    vec2 sdf3d_hit5 = vec2(sdf_node_10(p), 10.0);
    vec2 sdf3d_hit6 = vec2(min(sdf3d_hit4.x, sdf3d_hit5.x), (sdf3d_hit4.x < sdf3d_hit5.x ? sdf3d_hit4.y : sdf3d_hit5.y));
    return sdf3d_hit6;
}

float sceneSDF(vec3 p)
{
    return sceneSDFWithId(p).x;
}

float sceneNodeSDF(int nodeId, vec3 p)
{
    switch (nodeId) {
    case 2: return sdf_node_2(p);
    case 4: return sdf_node_4(p);
    case 7: return sdf_node_7(p);
    case 9: return sdf_node_9(p);
    case 12: return sdf_node_12(p);
    case 10: return sdf_node_10(p);
    case 5: return sdf_node_5(p);
    default: return 1e6;
    }
}

int scenePickId(vec3 p)
{
    return int(sceneSDFWithId(p).y);
}

bool sceneNodeContains(int nodeId, int visibleNodeId)
{
    switch (visibleNodeId) {
    case 5: return nodeId == 5 || nodeId == 2 || nodeId == 4 || nodeId == 7 || nodeId == 10 || nodeId == 12 || nodeId == 9;
    case 2: return nodeId == 2;
    case 4: return nodeId == 4;
    case 7: return nodeId == 7;
    case 10: return nodeId == 10 || nodeId == 12 || nodeId == 9;
    case 12: return nodeId == 12 || nodeId == 9;
    case 9: return nodeId == 9;
    default: return nodeId == visibleNodeId;
    }
}

SdfMaterialSample sceneMaterial(vec3 p)
{
    float sdf3d_distance0 = sdf_node_2(p);
    SdfMaterialSample sdf3d_material1 = sampleMaterial(0, p);
    float sdf3d_distance2 = sdf_node_4(p);
    SdfMaterialSample sdf3d_material3 = sampleMaterial(0, p);
    SdfMaterialSample sdf3d_material4 = selectMaterial(sdf3d_distance0 < sdf3d_distance2, sdf3d_material1, sdf3d_material3);
    float sdf3d_distance5 = min(sdf3d_distance0, sdf3d_distance2);
    float sdf3d_distance6 = sdf_node_7(p);
    SdfMaterialSample sdf3d_material7 = sampleMaterial(0, p);
    SdfMaterialSample sdf3d_material8 = selectMaterial(sdf3d_distance5 < sdf3d_distance6, sdf3d_material4, sdf3d_material7);
    float sdf3d_distance9 = min(sdf3d_distance5, sdf3d_distance6);
    float sdf3d_distance10 = sdf_node_10(p);
    SdfMaterialSample sdf3d_material11 = sdf3d_material_1((p - uNodeParams[4].data0.xyz));
    SdfMaterialSample sdf3d_material12 = selectMaterial(sdf3d_distance9 < sdf3d_distance10, sdf3d_material8, sdf3d_material11);
    float sdf3d_distance13 = min(sdf3d_distance9, sdf3d_distance10);
    return sdf3d_material12;
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
    return uEnvColor;
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

vec3 tangentToWorld(vec3 local, vec3 normal)
{
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    return normalize(tangent * local.x + bitangent * local.y + normal * local.z);
}

vec3 worldToTangent(vec3 world, vec3 normal)
{
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    return vec3(dot(world, tangent), dot(world, bitangent), dot(world, normal));
}

vec3 sampleGGXVNDFHalfVector(vec3 viewDirection, vec3 normal, float roughness, inout uint rng)
{
    float alpha = max(roughness * roughness, 0.001);
    vec3 localView = worldToTangent(viewDirection, normal);
    localView.z = max(localView.z, 0.001);
    localView = normalize(localView);

    vec3 stretchedView = normalize(vec3(alpha * localView.x, alpha * localView.y, localView.z));
    float lensq = stretchedView.x * stretchedView.x + stretchedView.y * stretchedView.y;
    vec3 tangent1 = lensq > 0.0 ? vec3(-stretchedView.y, stretchedView.x, 0.0) * inversesqrt(lensq) : vec3(1.0, 0.0, 0.0);
    vec3 tangent2 = cross(stretchedView, tangent1);

    float r = sqrt(random01(rng));
    float phi = 2.0 * PI * random01(rng);
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + stretchedView.z);
    t2 = mix(sqrt(max(0.0, 1.0 - t1 * t1)), t2, s);

    vec3 normalSample = t1 * tangent1 + t2 * tangent2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * stretchedView;
    vec3 localHalf = normalize(vec3(alpha * normalSample.x, alpha * normalSample.y, max(0.0, normalSample.z)));
    return tangentToWorld(localHalf, normal);
}

vec3 sampleGlossyReflection(vec3 incomingDirection, vec3 normal, float roughness, inout uint rng)
{
    vec3 viewDirection = normalize(-incomingDirection);
    vec3 halfVector = sampleGGXVNDFHalfVector(viewDirection, normal, roughness, rng);
    vec3 sampled = reflect(incomingDirection, halfVector);
    if (dot(sampled, normal) <= 0.0) {
        sampled = reflect(incomingDirection, normal);
    }
    return normalize(sampled);
}

float cosineHemispherePdf(vec3 normal, vec3 direction)
{
    return max(dot(normal, direction), 0.0) / PI;
}

float powerHeuristic(float firstPdf, float secondPdf)
{
    float first = firstPdf * firstPdf;
    float second = secondPdf * secondPdf;
    return first / max(first + second, MIN_PDF);
}

vec3 diffuseBrdf(SdfMaterialSample material)
{
    return material.albedo / PI;
}

vec3 emissiveRadiance(SdfMaterialSample material)
{
    vec3 radiance = material.albedo * max(material.emission, 0.0);
    return min(radiance, vec3(MAX_EMISSIVE_RADIANCE));
}

vec3 glossyLobeEstimate(vec3 normal, vec3 viewDirection, vec3 lightDirection, SdfMaterialSample material)
{
    vec3 halfVector = normalize(viewDirection + lightDirection);
    float nDotH = max(dot(normal, halfVector), 0.0);
    float shininess = mix(96.0, 6.0, material.roughness);
    vec3 fresnel = mix(vec3(0.04), material.albedo, material.metallic);
    return fresnel * pow(nDotH, shininess) * (1.0 - material.roughness);
}

float bsdfPdfEstimate(vec3 normal, vec3 direction, SdfMaterialSample material)
{
    float diffusePdf = cosineHemispherePdf(normal, direction);
    float glossyPdf = mix(diffusePdf, 1.0, material.metallic) * (1.0 - material.roughness);
    return mix(diffusePdf, max(glossyPdf, MIN_PDF), material.metallic);
}

vec3 brdfEstimate(vec3 normal, vec3 viewDirection, vec3 lightDirection, SdfMaterialSample material)
{
    vec3 brdf = diffuseBrdf(material) * (1.0 - material.metallic);
    brdf += glossyLobeEstimate(normal, viewDirection, lightDirection, material);
    return brdf;
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

vec3 directionalLightEstimate(vec3 hitPosition, vec3 normal, vec3 viewDirection, SdfMaterialSample material)
{
    vec3 lightDirection = DIRECT_LIGHT_DIRECTION;
    float nDotL = max(dot(normal, lightDirection), 0.0);
    if (nDotL <= 0.0 || !lightVisible(hitPosition + normal * SURFACE_EPSILON * 4.0, lightDirection, MAX_DISTANCE)) {
        return vec3(0.0);
    }

    vec3 brdf = brdfEstimate(normal, viewDirection, lightDirection, material);
    float bsdfPdf = bsdfPdfEstimate(normal, lightDirection, material);
    float misWeight = powerHeuristic(DIRECT_LIGHT_PDF, bsdfPdf);
    return DIRECT_LIGHT_RADIANCE * brdf * nDotL * misWeight / DIRECT_LIGHT_PDF;
}

vec3 stochasticEmissiveSurfaceEstimate(vec3 hitPosition, vec3 normal, vec3 viewDirection, SdfMaterialSample material, inout uint rng)
{
    vec3 estimate = vec3(0.0);
    for (int i = 0; i < EMISSIVE_SURFACE_HIT_SAMPLES; ++i) {
        vec3 lightDirection = cosineHemisphere(normal, rng);
        float lightPdf = cosineHemispherePdf(normal, lightDirection);
        if (lightPdf <= MIN_PDF) {
            continue;
        }

        vec3 lightHitPosition = vec3(0.0);
        float lightDistance = raymarch(hitPosition + normal * SURFACE_EPSILON * 4.0, lightDirection, lightHitPosition);
        if (lightDistance < 0.0) {
            continue;
        }

        SdfMaterialSample lightMaterial = sceneMaterial(lightHitPosition);
        vec3 lightRadiance = emissiveRadiance(lightMaterial);
        if (max(max(lightRadiance.r, lightRadiance.g), lightRadiance.b) <= 0.0) {
            continue;
        }

        vec3 lightNormal = estimateNormal(lightHitPosition);
        if (dot(lightNormal, -lightDirection) <= 0.0) {
            continue;
        }

        float nDotL = max(dot(normal, lightDirection), 0.0);
        vec3 brdf = brdfEstimate(normal, viewDirection, lightDirection, material);
        float bsdfPdf = bsdfPdfEstimate(normal, lightDirection, material);
        float misWeight = powerHeuristic(lightPdf, bsdfPdf);
        estimate += lightRadiance * brdf * nDotL * misWeight / lightPdf;
    }
    return estimate / float(EMISSIVE_SURFACE_HIT_SAMPLES);
}

vec3 nextEventEstimate(vec3 hitPosition, vec3 normal, vec3 viewDirection, SdfMaterialSample material, inout uint rng)
{
    vec3 estimate = directionalLightEstimate(hitPosition, normal, viewDirection, material);
    estimate += stochasticEmissiveSurfaceEstimate(hitPosition, normal, viewDirection, material, rng);
    return estimate;
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
        radiance += throughput * emissiveRadiance(material);
        radiance += throughput * nextEventEstimate(hitPosition, normal, viewDirection, material, rng);

        bool sampleMetal = random01(rng) < material.metallic;
        if (sampleMetal) {
            rayDirection = sampleGlossyReflection(rayDirection, normal, material.roughness, rng);
            throughput *= mix(vec3(0.04), material.albedo, material.metallic);
        } else {
            rayDirection = cosineHemisphere(normal, rng);
            throughput *= material.albedo;
        }

        rayOrigin = hitPosition + normal * SURFACE_EPSILON * 4.0;
        if (bounce >= RUSSIAN_ROULETTE_START_BOUNCE) {
            float keep = clamp(max(max(throughput.r, throughput.g), throughput.b), RUSSIAN_ROULETTE_MIN_KEEP, RUSSIAN_ROULETTE_MAX_KEEP);
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
