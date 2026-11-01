#version 330

#define MAX_POINT_LIGHTS 16

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3 viewPos;
uniform vec3 keyDirection;
uniform vec3 keyColor;
uniform vec3 skyAmbient;
uniform vec3 groundAmbient;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float emissive;
uniform vec3 glow;

uniform mat4 lightVP;
uniform sampler2D shadowMap;
uniform int shadowMapResolution;
uniform float shadowStrength;

uniform int pointCount;
uniform vec3 pointPosition[MAX_POINT_LIGHTS];
uniform vec3 pointColor[MAX_POINT_LIGHTS];
uniform float pointRadius[MAX_POINT_LIGHTS];

out vec4 finalColor;

vec3 toLinear(vec3 color)
{
    return pow(color, vec3(2.2));
}

vec3 tonemap(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

float visibility(vec3 normal, float ndl)
{
    vec3 offsetPosition = fragPosition + normal * 0.02;
    vec4 lightSpace = lightVP * vec4(offsetPosition, 1.0);
    vec3 coords = lightSpace.xyz / lightSpace.w * 0.5 + 0.5;

    if (coords.x <= 0.0 || coords.x >= 1.0 || coords.y <= 0.0 || coords.y >= 1.0 || coords.z >= 1.0) {
        return 1.0;
    }

    float bias = max(0.0025 * (1.0 - ndl), 0.0006);
    vec2 texel = vec2(1.0 / float(shadowMapResolution));
    float lit = 0.0;

    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            float depth = texture(shadowMap, coords.xy + texel * vec2(x, y)).r;
            lit += coords.z - bias > depth ? 0.0 : 1.0;
        }
    }

    return mix(1.0, lit / 25.0, shadowStrength);
}

void main()
{
    vec4 base = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (base.a < 0.35) {
        discard;
    }

    vec3 albedo = toLinear(base.rgb);
    vec3 normal = normalize(fragNormal);
    if (!gl_FrontFacing) {
        normal = -normal;
    }
    vec3 view = normalize(viewPos - fragPosition);

    float ndl = max(dot(normal, keyDirection), 0.0);
    float shadow = visibility(normal, ndl);

    vec3 ambient = mix(groundAmbient, skyAmbient, normal.y * 0.5 + 0.5);
    vec3 diffuse = keyColor * ndl * shadow;

    vec3 halfway = normalize(keyDirection + view);
    float specular = pow(max(dot(normal, halfway), 0.0), 48.0) * 0.12 * ndl * shadow;

    vec3 local = vec3(0.0);
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        if (i >= pointCount) {
            break;
        }
        vec3 toLight = pointPosition[i] - fragPosition;
        float distance = length(toLight);
        float attenuation = clamp(1.0 - distance / pointRadius[i], 0.0, 1.0);
        attenuation *= attenuation;
        float wrap = max(dot(normal, toLight / max(distance, 0.0001)) * 0.7 + 0.3, 0.0);
        local += pointColor[i] * attenuation * wrap;
    }

    float rim = pow(1.0 - max(dot(normal, view), 0.0), 3.0) * 0.35;

    vec3 color = albedo * (diffuse + ambient + local)
               + keyColor * specular
               + rim * skyAmbient * albedo
               + albedo * emissive * 4.0
               + glow * (pow(1.0 - max(dot(normal, view), 0.0), 2.0) * 1.6 + 0.08);

    float distanceToEye = length(viewPos - fragPosition);
    float fog = 1.0 - exp(-pow(distanceToEye * fogDensity, 2.0));
    color = mix(color, fogColor, clamp(fog, 0.0, 1.0));

    color = tonemap(color * 1.1);
    finalColor = vec4(pow(color, vec3(1.0 / 2.2)), base.a);
}
