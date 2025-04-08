#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "scene.glsl"
#include "mesh_structures.glsl"

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform constants {
    mat4 renderMatrix;
    VertexBuffer vertexBuffer;
} pushConstants;

void main()
{
    Vertex v = pushConstants.vertexBuffer.vertices[gl_VertexIndex];

    vec4 worldPos = pushConstants.renderMatrix * vec4(v.position, 1.0f);
    vec3 worldNormal = normalize((pushConstants.renderMatrix * vec4(v.normal, 0.0)).xyz);

    // Apply offset
    // TODO: Make offset configurable
    worldPos.xyz += worldNormal * 0.1f;

    gl_Position = sceneData.viewproj * worldPos;
}
