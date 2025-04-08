#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"
#include "mesh_structures.glsl"
#include "scene.glsl"

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outUV;
layout(location = 3) out vec4 outPosLightSpace;

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform constants
{
    mat4 renderMatrix;
    VertexBuffer vertexBuffer;
} pushConstants;

void main()
{
    Vertex v = pushConstants.vertexBuffer.vertices[gl_VertexIndex];

    vec4 worldPos = pushConstants.renderMatrix * vec4(v.position, 1.0f);

    gl_Position = sceneData.viewproj * worldPos;

    outNormal = (pushConstants.renderMatrix * vec4(v.normal, 0.f)).xyz;
    outColor = v.color.xyz;
    outUV.x = v.uv_x;
    outUV.y = v.uv_y;
    outPosLightSpace = sceneData.lightViewproj * worldPos;
}
