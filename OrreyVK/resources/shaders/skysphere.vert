#version 450
#define SKYSPHERESCALE 500000

layout(location = 0) in vec3 vtxPosIn;
layout(location = 1) in vec3 vtxColourIn;
layout(location = 2) in vec3 vtxUVIn;

layout(location = 1) out vec3 fragColourIn;
layout(location = 2) out vec3 fragUVIn;

layout (binding = 2) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
} ubo;

void main() {

	vec4 pos = vec4(vtxPosIn.xyz * SKYSPHERESCALE, 1.0);
	gl_Position = ubo.projection * ubo.view * ubo.model * pos;

    fragColourIn = vtxColourIn;
	fragUVIn = vtxUVIn;
}