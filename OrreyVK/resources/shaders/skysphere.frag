#version 450

layout (binding = 3) uniform sampler2D imageSampler;

layout(location = 0) out vec4 outColor;
layout(location = 1) in vec3 fragColourIn;
layout(location = 2) in vec3 fragUVIn;

void main() {
    outColor = texture(imageSampler, fragUVIn.xy);
}