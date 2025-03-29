#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "scene.glsl"

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform constants {
    mat4 model;
    VertexBuffer vertexBuffer;
} PushConstants;

void main() {
    Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

    // Only need position for shadow mapping
    gl_Position = sceneData.lightViewproj * PushConstants.model * vec4(v.position, 1.0);
}
