#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "tile_structures.glsl"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;

struct InstanceData {
	mat4 modelMatrix;
	vec3 color;
}; 

layout(buffer_reference, std430) readonly buffer InstanceDataBuffer{ 
	InstanceData instances[];
};

layout( push_constant ) uniform constants {	
	InstanceDataBuffer instanceBuffer;
} PushConstants;

void main() {	
	InstanceData instance = PushConstants.instanceBuffer.instances[gl_InstanceIndex];

	gl_Position = sceneData.viewproj * instance.modelMatrix * vec4(inPosition, 1.0);

	outNormal = inNormal;
	outColor = v.color.xyz;
}

