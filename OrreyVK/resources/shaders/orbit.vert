#version 450

layout(location = 0) in vec2 vtxPosIn;

layout (binding = 2) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
} ubo;

void main() {

	vec4 pos = vec4(vtxPosIn.x, 0.0, vtxPosIn.y, 1.0);
	gl_Position = ubo.projection * ubo.view * ubo.model * pos;
}