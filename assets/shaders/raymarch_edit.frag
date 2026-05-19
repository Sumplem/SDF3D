#version 460 core

out vec4 outColor;

uniform vec2 uResolution;
uniform vec3 uCameraPosition;
uniform vec3 uCameraTarget;
uniform vec3 uCameraUp;
uniform float uFovDegrees;
uniform int uMaterialCount;

uniform bool uGizmoVisible;
uniform vec3 uGizmoCenter;
uniform mat3 uGizmoOrientation;
uniform float uGizmoArrowLength;
uniform float uGizmoArrowRadius;
uniform float uGizmoR;
uniform float uGizmoTube;
uniform int uGizmoActiveAxis;
uniform int uGizmoHoverAxis;
uniform int uGizmoType;
uniform int uGizmoRotateStyle;
uniform int uHighlightNodeId;
uniform int uRenderQuality;

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
const float SHADOW_MIN_DISTANCE = 0.01;
const float SHADOW_MAX_DISTANCE = 32.0;
const float SHADOW_SOFTNESS = 10.0;
const int AO_STEPS = 5;
const float AO_STEP_SIZE = 0.08;
const float AO_STRENGTH = 1.4;
const int GIZMO_AXIS_NONE = -1;
const int GIZMO_TYPE_TRANSLATE = 0;
const int GIZMO_TYPE_ROTATE = 1;
const int GIZMO_TYPE_SCALE = 2;
const int GIZMO_ROTATE_STYLE_RINGS = 0;
const int GIZMO_ROTATE_STYLE_AXIS_ARCS = 1;
const int QUALITY_LOW = 0;
const int QUALITY_MEDIUM = 1;
const int QUALITY_HIGH = 2;

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

int raymarchStepLimit()
{
    if (uRenderQuality <= QUALITY_LOW) {
        return 48;
    }
    if (uRenderQuality == QUALITY_MEDIUM) {
        return 80;
    }
    return MAX_STEPS;
}

int shadowStepLimit()
{
    if (uRenderQuality <= QUALITY_LOW) {
        return 0;
    }
    if (uRenderQuality == QUALITY_MEDIUM) {
        return 48;
    }
    return MAX_STEPS;
}

int ambientOcclusionStepLimit()
{
    if (uRenderQuality <= QUALITY_LOW) {
        return 0;
    }
    if (uRenderQuality == QUALITY_MEDIUM) {
        return 3;
    }
    return AO_STEPS;
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
    return sampleMaterial(0, p);
}

float sceneNodeSDF(int nodeId, vec3 p)
{
    return 1e6;
}
// SDF3D_SCENE_END

float sdCapsule(vec3 p, vec3 a, vec3 b, float radius)
{
    vec3 pa = p - a;
    vec3 ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 0.0001), 0.0, 1.0);
    return length(pa - ba * h) - radius;
}

float sdTorus(vec3 p, vec2 t)
{
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float sdBox(vec3 p, vec3 halfSize)
{
    vec3 q = abs(p) - halfSize;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float nearestAxisDistance(float xDistance, float yDistance, float zDistance, out int axis)
{
    axis = GIZMO_AXIS_NONE;
    float distanceToGizmo = xDistance;
    axis = 0;
    if (yDistance < distanceToGizmo) {
        distanceToGizmo = yDistance;
        axis = 1;
    }
    if (zDistance < distanceToGizmo) {
        distanceToGizmo = zDistance;
        axis = 2;
    }

    return distanceToGizmo;
}

float translateGizmoSDF(vec3 p, out int axis)
{
    float radius = max(uGizmoArrowRadius, 0.0001);
    float length = max(uGizmoArrowLength, radius);
    vec3 center = uGizmoCenter;

    float xDistance = sdCapsule(p, center, center + vec3(length, 0.0, 0.0), radius);
    float yDistance = sdCapsule(p, center, center + vec3(0.0, length, 0.0), radius);
    float zDistance = sdCapsule(p, center, center + vec3(0.0, 0.0, length), radius);
    return nearestAxisDistance(xDistance, yDistance, zDistance, axis);
}

float rotateGizmoSDF(vec3 p, out int axis)
{
    vec3 local = transpose(uGizmoOrientation) * (p - uGizmoCenter);
    vec2 torus = vec2(max(uGizmoR, 0.0001), max(uGizmoTube, 0.0001));
    float xDistance = sdTorus(local.yxz, torus);
    float yDistance = sdTorus(local, torus);
    float zDistance = sdTorus(local.xzy, torus);
    if (uGizmoRotateStyle == GIZMO_ROTATE_STYLE_AXIS_ARCS) {
        // STUB: AxisArcs currently uses ring SDF until arc sector clipping is implemented.
        return nearestAxisDistance(xDistance, yDistance, zDistance, axis);
    }
    return nearestAxisDistance(xDistance, yDistance, zDistance, axis);
}

float scaleGizmoSDF(vec3 p, out int axis)
{
    float length = max(uGizmoArrowLength, 0.0001);
    vec3 halfSize = vec3(max(uGizmoArrowRadius * 2.5, 0.0001));
    vec3 center = uGizmoCenter;

    vec3 xAxis = uGizmoOrientation * vec3(1.0, 0.0, 0.0);
    vec3 yAxis = uGizmoOrientation * vec3(0.0, 1.0, 0.0);
    vec3 zAxis = uGizmoOrientation * vec3(0.0, 0.0, 1.0);
    float xDistance = sdBox(transpose(uGizmoOrientation) * (p - (center + xAxis * length)), halfSize);
    float yDistance = sdBox(transpose(uGizmoOrientation) * (p - (center + yAxis * length)), halfSize);
    float zDistance = sdBox(transpose(uGizmoOrientation) * (p - (center + zAxis * length)), halfSize);
    return nearestAxisDistance(xDistance, yDistance, zDistance, axis);
}

float gizmoSDF(vec3 p, out int axis)
{
    axis = GIZMO_AXIS_NONE;
    if (!uGizmoVisible) {
        return MAX_DISTANCE;
    }
    if (uGizmoType == GIZMO_TYPE_TRANSLATE) {
        return translateGizmoSDF(p, axis);
    }
    if (uGizmoType == GIZMO_TYPE_ROTATE) {
        return rotateGizmoSDF(p, axis);
    }
    if (uGizmoType == GIZMO_TYPE_SCALE) {
        return scaleGizmoSDF(p, axis);
    }
    return MAX_DISTANCE;
}

vec3 gizmoAxisColor(int axis)
{
    vec3 color = vec3(0.85);
    if (axis == 0) {
        color = vec3(0.92, 0.22, 0.18);
    } else if (axis == 1) {
        color = vec3(0.22, 0.82, 0.32);
    } else if (axis == 2) {
        color = vec3(0.20, 0.42, 0.95);
    }

    if (axis == uGizmoHoverAxis) {
        color = mix(color, vec3(1.0), 0.28);
    }
    if (axis == uGizmoActiveAxis) {
        color = mix(color, vec3(1.0), 0.48);
    }

    return color;
}

vec3 estimateSceneNormal(vec3 p)
{
    vec2 e = vec2(NORMAL_EPSILON, 0.0);
    return normalize(vec3(
        sceneSDF(p + e.xyy) - sceneSDF(p - e.xyy),
        sceneSDF(p + e.yxy) - sceneSDF(p - e.yxy),
        sceneSDF(p + e.yyx) - sceneSDF(p - e.yyx)
    ));
}

vec3 estimateGizmoNormal(vec3 p)
{
    vec2 e = vec2(NORMAL_EPSILON, 0.0);
    int axis = GIZMO_AXIS_NONE;
    return normalize(vec3(
        gizmoSDF(p + e.xyy, axis) - gizmoSDF(p - e.xyy, axis),
        gizmoSDF(p + e.yxy, axis) - gizmoSDF(p - e.yxy, axis),
        gizmoSDF(p + e.yyx, axis) - gizmoSDF(p - e.yyx, axis)
    ));
}

float raymarchScene(vec3 rayOrigin, vec3 rayDirection, out vec3 hitPosition)
{
    float distanceTraveled = 0.0;
    int stepLimit = raymarchStepLimit();

    for (int i = 0; i < MAX_STEPS; ++i) {
        if (i >= stepLimit) {
            break;
        }
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

float raymarchGizmo(vec3 rayOrigin, vec3 rayDirection, out vec3 hitPosition, out int hitAxis)
{
    float distanceTraveled = 0.0;
    hitAxis = GIZMO_AXIS_NONE;
    int stepLimit = raymarchStepLimit();

    for (int i = 0; i < MAX_STEPS; ++i) {
        if (i >= stepLimit) {
            break;
        }
        hitPosition = rayOrigin + rayDirection * distanceTraveled;
        int axis = GIZMO_AXIS_NONE;
        float distanceToGizmo = gizmoSDF(hitPosition, axis);

        if (axis != GIZMO_AXIS_NONE && distanceToGizmo < SURFACE_EPSILON) {
            hitAxis = axis;
            return distanceTraveled;
        }

        distanceTraveled += max(distanceToGizmo, SURFACE_EPSILON);
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
    int stepLimit = shadowStepLimit();
    if (stepLimit == 0) {
        return 1.0;
    }

    for (int i = 0; i < MAX_STEPS; ++i) {
        if (i >= stepLimit) {
            break;
        }
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
    int stepLimit = ambientOcclusionStepLimit();
    if (stepLimit == 0) {
        return 1.0;
    }

    for (int i = 1; i <= AO_STEPS; ++i) {
        if (i > stepLimit) {
            break;
        }
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
    vec3 sceneHitPosition = vec3(0.0);
    vec3 gizmoHitPosition = vec3(0.0);
    int gizmoAxis = GIZMO_AXIS_NONE;

    float sceneHitDistance = raymarchScene(rayOrigin, rayDirection, sceneHitPosition);
    float gizmoHitDistance = raymarchGizmo(rayOrigin, rayDirection, gizmoHitPosition, gizmoAxis);

    // AGENT: Gizmo intentionally ignores scene depth so edit handles stay
    // visible and clickable even when scene SDF lies between camera and gizmo.
    if (gizmoHitDistance >= 0.0) {
        vec3 normal = estimateGizmoNormal(gizmoHitPosition);
        vec3 lightDirection = normalize(vec3(-0.4, 0.7, 0.5));
        vec3 viewDirection = normalize(rayOrigin - gizmoHitPosition);
        float facing = 0.45 + 0.55 * max(dot(normal, lightDirection), 0.0);
        vec3 color = gizmoAxisColor(gizmoAxis) * facing;
        color += vec3(0.08) * max(dot(normal, viewDirection), 0.0);
        outColor = vec4(color, 1.0);
        return;
    }

    if (sceneHitDistance < 0.0) {
        outColor = vec4(backgroundColor(rayOrigin, rayDirection), 1.0);
        return;
    }

    vec3 normal = estimateSceneNormal(sceneHitPosition);
    vec3 lightDirection = normalize(vec3(-0.4, 0.7, 0.5));
    vec3 viewDirection = normalize(rayOrigin - sceneHitPosition);
    float shadow = softShadow(sceneHitPosition + normal * SURFACE_EPSILON * 2.0, lightDirection);
    float occlusion = ambientOcclusion(sceneHitPosition, normal);
    SdfMaterialSample material = sceneMaterial(sceneHitPosition);
    vec3 baseColor = material.albedo;
    float roughness = material.roughness;
    float metallic = material.metallic;

    vec3 ambient = baseColor * 0.18 * occlusion * (1.0 - metallic * 0.35);
    vec3 direct = pbrDirectLighting(normal, viewDirection, lightDirection, baseColor, roughness, metallic) * shadow;
    vec3 color = ambient + direct + baseColor * material.emission;
    if (uHighlightNodeId > 0) {
        float highlightDistance = abs(sceneNodeSDF(uHighlightNodeId, sceneHitPosition));
        float highlightMask = 1.0 - smoothstep(0.0, 0.06, highlightDistance);
        float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.0);
        vec3 highlight = vec3(1.0, 0.82, 0.12);
        color = mix(color, highlight, highlightMask * (0.35 + 0.40 * rim));
    }

    outColor = vec4(color, 1.0);
}
