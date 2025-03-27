#version 450

layout(location = 0) in vec3 vPosition;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    mat4 lightViewproj;
    vec4 ambientColor;
    vec4 sunlightColor;
    vec4 sunlightDirection;
} sceneData;

layout(push_constant) uniform constants {
    // uint64_t vertexBuffer;
    mat4 model;
} pushConstants;

void main() {
    // Only need position for shadow mapping
    gl_Position = sceneData.lightViewproj * pushConstants.model * vec4(vPosition, 1.0);
}

