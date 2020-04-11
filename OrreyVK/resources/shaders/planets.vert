#version 450

layout(location = 0) in vec3 vtxPosIn;
layout(location = 1) in vec3 vtxColourIn;
layout(location = 2) in vec3 vtxUVIn;

layout(location = 3) in vec4 instancePosIn;
layout(location = 4) in vec4 posOffset;
layout(location = 5) in vec4 scale;
layout(location = 6) in vec4 rotation;
layout(location = 7) in vec4 orbitalTilt;
layout(location = 8) in vec4 colourTint;

layout(location = 1) out vec3 fragColourIn;
layout(location = 2) out vec3 fragUVIn;

layout (binding = 2) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
} ubo;

mat3 GetRotationMatrix(vec3 rotation)
{
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

	return matZ * matY * matX; //Perform X rotation before Y
}

void main() {

	mat3 localRotMat =  GetRotationMatrix(rotation.xyz);
	mat3 orbitalTiltMat = GetRotationMatrix(orbitalTilt.xyz);

	localRotMat *= orbitalTiltMat;
		
	vec4 locPos = vec4(vtxPosIn.xyz * localRotMat, 1.0);
	vec4 pos = vec4((locPos.xyz * scale.xyz + instancePosIn.xyz), 1.0);
	pos.xyz *= orbitalTiltMat;
	pos.xyz += posOffset.xyz;
	
	gl_Position = ubo.projection * ubo.view * ubo.model * pos;
    
	fragColourIn = colourTint.xyz;
	fragUVIn = vtxUVIn;
	fragUVIn.z = scale.w;
}