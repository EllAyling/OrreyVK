#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (binding = 3) uniform sampler2DArray samplerArray;

layout(location = 0) out vec4 outColor;
layout(location = 1) in vec3 fragColourIn;
layout(location = 2) in vec3 fragUVIn;

void main() {

    outColor = texture(samplerArray, fragUVIn);
    outColor.xyz *= fragColourIn;

    if(fragUVIn.z < 0.0)
    {
        outColor.xyz = fragColourIn;
        outColor.w = 1.0;
    }
   
}