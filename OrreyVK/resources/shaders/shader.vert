#version 450

layout(location = 0) in vec3 vtxPosIn;
layout(location = 1) in vec3 vtxColourIn;

layout(location = 2) in vec4 instancePosIn;
layout(location = 3) in vec4 scale;

layout(location = 1) out vec3 fragColourIn;

layout (binding = 2) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
} ubo;

void main() {

	mat3 matY;
	float s = sin(180.0);
	float c = cos(180.0);
	
	matY[0] = vec3(c, 0.0, s);
	matY[1] = vec3(0.0, 1.0, 0.0);
	matY[2] = vec3(-s, 0.0, c);

	mat3 matZ;
	s = sin(90.0);
	c = cos(90.0);	
	
	matZ[0] = vec3(1.0, 0.0, 0.0);
	matZ[1] = vec3(0.0, c, s);
	matZ[2] = vec3(0.0, -s, c);

	mat3 rotMat = matZ * matY;
	
	mat4 gRotMat;
	s = sin(180.0);
	c = cos(180.0);
	gRotMat[0] = vec4(c, 0.0, s, 0.0);
	gRotMat[1] = vec4(0.0, 1.0, 0.0, 0.0);
	gRotMat[2] = vec4(-s, 0.0, c, 0.0);
	gRotMat[3] = vec4(0.0, 0.0, 0.0, 1.0);	

	vec4 locPos = vec4(vtxPosIn.xyz * rotMat, 1.0);
	vec4 pos = vec4((locPos.xyz * scale.x) + instancePosIn.xyz, 1.0);
	
	gl_Position = ubo.projection * ubo.view * ubo.model * pos;
    
	fragColourIn = vtxColourIn;
}