#version 450
#extension GL_GOOGLE_include_directive : require

#include "scene.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 2) in vec3 inInstancePos;
layout(location = 3) in vec3 inInstanceColor;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outColor;

layout(push_constant) uniform constants {
    mat4 chunkModel;
} PushConstants;

void main() {
    vec3 worldPos = inPosition + inInstancePos;

    gl_Position = sceneData.viewproj * PushConstants.chunkModel * vec4(worldPos, 1.0);

    outNormal = inNormal;
    outColor = inInstanceColor;
}
