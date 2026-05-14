#version 460 core

out vec4 outColor;

uniform vec2 uResolution;
uniform vec3 uCameraPosition;
uniform vec3 uCameraTarget;
uniform vec3 uCameraUp;
uniform float uFovDegrees;
uniform int uMaterialCount;
uniform vec3 uMaterialAlbedo[64];
uniform float uMaterialRoughness[64];
uniform float uMaterialMetallic[64];
uniform float uMaterialEmission[64];

const int MAX_STEPS = 128;
const float MAX_DISTANCE = 100.0;
const float SURFACE_EPSILON = 0.001;
const float PI = 3.14159265358979323846;
const int MAX_MATERIALS = 64;

// AGENT: Renderer replaces the block between these markers with GLSL emitted
// by SdfCompiler when the scene graph changes.
// SDF3D_SCENE_BEGIN
vec2 sceneSDFWithMaterial(vec3 p)
{
    return vec2(length(p) - 1.0, 0.0);
}

float sceneSDF(vec3 p)
{
    return sceneSDFWithMaterial(p).x;
}
// SDF3D_SCENE_END

vec3 estimateNormal(vec3 p)
{
    // AGENT: Central differences are cheap and stable enough for the M2
    // hardcoded scene; analytic normals can be introduced per-node later.
    vec2 e = vec2(SURFACE_EPSILON, 0.0);
    return normalize(vec3(
        sceneSDF(p + e.xyy) - sceneSDF(p - e.xyy),
        sceneSDF(p + e.yxy) - sceneSDF(p - e.yxy),
        sceneSDF(p + e.yyx) - sceneSDF(p - e.yyx)
    ));
}

float raymarch(vec3 rayOrigin, vec3 rayDirection, out vec3 hitPosition, out int materialId)
{
    float distanceTraveled = 0.0;
    materialId = 0;

    for (int i = 0; i < MAX_STEPS; ++i) {
        hitPosition = rayOrigin + rayDirection * distanceTraveled;
        vec2 sceneSample = sceneSDFWithMaterial(hitPosition);
        float distanceToScene = sceneSample.x;

        if (distanceToScene < SURFACE_EPSILON) {
            materialId = int(clamp(floor(sceneSample.y + 0.5), 0.0, float(MAX_MATERIALS - 1)));
            return distanceTraveled;
        }

        distanceTraveled += distanceToScene;
        if (distanceTraveled > MAX_DISTANCE) {
            break;
        }
    }

    return -1.0;
}

vec3 materialAlbedo(int materialId)
{
    if (materialId < 0 || materialId >= uMaterialCount || materialId >= MAX_MATERIALS) {
        return vec3(0.78, 0.82, 0.88);
    }

    return uMaterialAlbedo[materialId];
}

float materialEmission(int materialId)
{
    if (materialId < 0 || materialId >= uMaterialCount || materialId >= MAX_MATERIALS) {
        return 0.0;
    }

    return uMaterialEmission[materialId];
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
    int materialId = 0;

    float hitDistance = raymarch(rayOrigin, rayDirection, hitPosition, materialId);
    if (hitDistance < 0.0) {
        outColor = vec4(backgroundColor(rayOrigin, rayDirection), 1.0);
        return;
    }

    vec3 normal = estimateNormal(hitPosition);
    vec3 lightDirection = normalize(vec3(-0.4, 0.7, 0.5));
    vec3 viewDirection = normalize(rayOrigin - hitPosition);
    vec3 halfVector = normalize(lightDirection + viewDirection);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(normal, halfVector), 0.0), 48.0);
    vec3 baseColor = materialAlbedo(materialId);
    vec3 color = baseColor * (0.18 + diffuse * 0.78) + vec3(0.35) * specular + baseColor * materialEmission(materialId);

    outColor = vec4(color, 1.0);
}
