#version 450

layout(location = 0) in vec3 vtxPosIn;
layout(location = 1) in vec3 vtxColourIn;
layout(location = 2) in vec3 vtxUVIn;

layout(location = 3) in vec4 instancePosIn;
layout(location = 4) in vec4 scale;
layout(location = 5) in vec4 rotation;

layout(location = 1) out vec3 fragColourIn;
layout(location = 2) out vec3 fragUVIn;

layout (binding = 2) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
} ubo;

void main() {

	mat3 matX;
	float s = sin(rotation.x);
	float c = cos(rotation.x);

	matX[0] = vec3(c, s, 0.0);
	matX[1] = vec3(-s, c, 0.0);
	matX[2] = vec3(0.0, 0.0, 1.0);

	mat3 matY;
	s = sin(rotation.y);
	c = cos(rotation.y);
	
	matY[0] = vec3(c, 0.0, s);
	matY[1] = vec3(0.0, 1.0, 0.0);
	matY[2] = vec3(-s, 0.0, c);

	mat3 matZ;
	s = sin(rotation.z);
	c = cos(rotation.z);
	
	matZ[0] = vec3(1.0, 0.0, 0.0);
	matZ[1] = vec3(0.0, c, s);
	matZ[2] = vec3(0.0, -s, c);

	mat3 rotMat =  matZ * matY * matX; //Perform X rotation before Y
	
	vec4 locPos = vec4(vtxPosIn.xyz * rotMat, 1.0);
	vec4 pos = vec4((locPos.xyz * scale.x) + instancePosIn.xyz, 1.0);
	
	gl_Position = ubo.projection * ubo.view * ubo.model * pos;
    
	fragColourIn = vtxColourIn;
	fragUVIn = vtxUVIn;
	fragUVIn.z = scale.w;
}