#version 450

layout(location = 0) in vec3 vtxPosIn;
layout(location = 1) in vec3 vtxColourIn;

layout(location = 1) out vec3 fragColourIn;

layout (binding = 2) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
} ubo;

void main() {
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(vtxPosIn, 1.0f);
    fragColourIn = vtxColourIn;
}