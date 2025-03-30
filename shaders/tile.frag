#version 450

#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec4 inPosLightSpace;

layout(location = 0) out vec4 outFragColor;

layout(set = 1, binding = 0) uniform sampler2DShadow shadowMapTex;

#include "scene.glsl"
#include "shadow.glsl"

void main() {
    float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);
    float shadow = calculateShadowFactor(inPosLightSpace, inNormal, sceneData.sunlightDirection, shadowMapTex);

    vec3 ambient = inColor * sceneData.ambientColor.xyz;
    vec3 color = inColor * shadow;

    // outFragColor = vec4(color * lightValue * sceneData.sunlightColor.w + ambient, 1.0f);

    vec3 projCoords = inPosLightSpace.xyz / inPosLightSpace.w; // Perspective divide -> NDC
    vec2 shadowTexCoord = projCoords.xy * 0.5 + 0.5;

    outFragColor = vec4(color * lightValue * sceneData.sunlightColor.w + ambient, 1.0f);
    // outFragColor = vec4(shadowTexCoord, 0, 1);
}
