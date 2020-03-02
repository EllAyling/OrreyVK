#include "SolidSphere.h"

SolidSphere::SolidSphere()
{

}

SolidSphere::SolidSphere(float radius, size_t stacks, size_t slices)		//Create the planet spheare with normals.
{
	// Adapated from: https://github.com/Erkaman/cute-deferred-shading/blob/master/src/main.cpp#L573

	// loop through stacks.
	for (int i = 0; i <= stacks; ++i) {

		float V = (float)i / (float)stacks;
		float phi = V * M_PI;

		// loop through the slices.
		for (int j = 0; j <= slices; ++j) {

			float U = (float)j / (float)slices;
			float theta = U * (M_PI * 2);

			// use spherical coordinates to calculate the positions.
			float x = cos(theta) * sin(phi) * radius;
			float y = cos(phi) * radius;
			float z = sin(theta) * sin(phi) * radius;

			vertices.push_back({ { x, y, z }, { x, y, z, 1.0 } });
		}
	}

	// Calc The Index Positions
	for (int i = 0; i < slices * stacks + slices; ++i) {
		indices.push_back(uint16_t(i));
		indices.push_back(uint16_t(i + slices + 1));
		indices.push_back(uint16_t(i + slices));

		indices.push_back(uint16_t(i + slices + 1));
		indices.push_back(uint16_t(i));
		indices.push_back(uint16_t(i + 1));
	}

	//vertices = std::vector<VulkanTools::VertexInput> {
	//{{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
	//{{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
	//{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
	//};
	//
	//indices = std::vector<uint16_t>{
	//0, 1, 2, 2, 3, 0
	//};
}

SolidSphere::~SolidSphere()
{

}

vk::VertexInputBindingDescription SolidSphere::GetVertexBindingDescription()
{
	return vk::VertexInputBindingDescription(0, sizeof(VulkanTools::VertexInput));
}

std::vector<vk::VertexInputAttributeDescription> SolidSphere::GetVertexAttributeDescription()
{
	return std::vector<vk::VertexInputAttributeDescription>{
						vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(VulkanTools::VertexInput, pos)),
						vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(VulkanTools::VertexInput, colour))
	};
}
