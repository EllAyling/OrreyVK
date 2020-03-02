#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;
layout(location = 1) in vec3 fragColourIn;

void main() {
    outColor = vec4(fragColourIn, 1.0);
}