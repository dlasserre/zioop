#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform vec2 resolution;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform vec3 zenithColor;
uniform vec3 horizonColor;
uniform float night;
uniform float time;

out vec4 finalColor;

float hash(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float hash2(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2(i), hash2(i + vec2(1.0, 0.0)), u.x),
               mix(hash2(i + vec2(0.0, 1.0)), hash2(i + vec2(1.0, 1.0)), u.x), u.y);
}

float fbm(vec2 p)
{
    float sum = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 5; i++) {
        sum += amplitude * noise(p);
        p *= 2.03;
        amplitude *= 0.5;
    }
    return sum;
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

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution * 2.0 - 1.0;
    float aspect = resolution.x / resolution.y;
    vec3 direction = normalize(cameraForward
                             + uv.x * aspect * tanHalfFov * cameraRight
                             + uv.y * tanHalfFov * cameraUp);
    float height = direction.y;

    vec3 color = mix(horizonColor, zenithColor, pow(clamp(height, 0.0, 1.0), 0.45));
    if (height < 0.0) {
        color = mix(horizonColor, horizonColor * 0.55, clamp(-height * 4.0, 0.0, 1.0));
    }

    float sunAbove = smoothstep(-0.12, 0.02, sunDirection.y);
    float sunDot = max(dot(direction, sunDirection), 0.0);
    color += sunColor * (smoothstep(0.9990, 0.9995, sunDot) * 8.0 + pow(sunDot, 10.0) * 0.35) * sunAbove;

    vec3 moonDirection = -sunDirection;
    float moonDot = max(dot(direction, moonDirection), 0.0);
    color += vec3(0.85, 0.9, 1.0) * (smoothstep(0.9992, 0.9996, moonDot) * 1.6 + pow(moonDot, 60.0) * 0.06) * night;

    if (height > 0.0) {
        vec3 cell = floor(direction * 220.0);
        float seed = hash(cell);
        float twinkle = 0.6 + 0.4 * sin(time * 2.5 + seed * 120.0);
        float star = step(0.9972, seed) * twinkle;
        color += vec3(star) * night * smoothstep(0.0, 0.25, height);
    }

    if (height > 0.01) {
        vec2 cloudPosition = direction.xz / (height + 0.12) * 1.4 + vec2(time * 0.012, time * 0.004);
        float cloud = smoothstep(0.48, 0.78, fbm(cloudPosition));
        vec3 litCloud = horizonColor * 1.25 + sunColor * 0.25;
        vec3 cloudColor = mix(litCloud, vec3(0.025, 0.03, 0.05), night);
        color = mix(color, cloudColor, cloud * 0.75 * smoothstep(0.01, 0.3, height));
    }

    color = tonemap(color * 1.1);
    finalColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
