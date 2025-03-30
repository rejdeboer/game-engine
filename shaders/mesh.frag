#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"
#include "scene.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inPosLightSpace;

layout(location = 0) out vec4 outFragColor;

// --- Shadow Calculation Function ---
float calculateShadowFactor() {
    // 1. Perspective Divide: Convert from clip space to NDC [-1, 1]
    vec3 projCoords = inPosLightSpace.xyz / inPosLightSpace.w;

    // 2. Transform to Texture Coordinates [0, 1]
    vec2 shadowTexCoords = projCoords.xy * 0.5 + 0.5;

    float bias = max(0.005 * tan(acos(dot(normalize(inNormal), normalize(sceneData.sunlightDirection.xyz)))), 0.001);
    float shadow = texture(shadowMapTex, vec3(shadowTexCoords, projCoords.z - bias));

    // Optional: Clamp coords or check depth bounds if needed
    // (Sampler border mode CLAMP_TO_BORDER with white often handles points outside the frustum)
    if (projCoords.z > 1.0) { // If beyond light's far plane
        shadow = 1.0; // Consider it lit
    }

    // TODO: Consider optionals
    // Optional: Basic PCF (Percentage-Closer Filtering) for softer shadows
    // float pcfShadow = 0.0;
    // vec2 texelSize = 1.0 / textureSize(shadowMapSampler, 0); // Get size of one texel
    // int pcfRadius = 1; // Sample a 3x3 area (radius 1)
    // for(int x = -pcfRadius; x <= pcfRadius; ++x) {
    //     for(int y = -pcfRadius; y <= pcfRadius; ++y) {
    //         pcfShadow += texture(shadowMapSampler, vec3(projCoords.xy + vec2(x, y) * texelSize, projCoords.z));
    //     }
    // }
    // shadow = pcfShadow / ((pcfRadius*2+1)*(pcfRadius*2+1)); // Average the results

    return shadow;
}

void main() {
    float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);
    float shadow = calculateShadowFactor();

    vec3 direct = inColor * texture(colorTex, inUV).xyz;
    vec3 ambient = direct * sceneData.ambientColor.xyz;
    vec3 color = direct * shadow;

    outFragColor = vec4(color * lightValue * sceneData.sunlightColor.w + ambient, 1.0f);
}
