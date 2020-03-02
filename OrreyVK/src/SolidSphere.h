#pragma once
#ifndef SOLIDSPHERE_H
#define SOLIDSPHERE_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <vector>
#include <vulkan/vulkan.hpp>
#include "Types.h"

class SolidSphere
{
private:
	std::vector<VulkanTools::VertexInput> vertices;
	std::vector<float> normals;
	std::vector<uint16_t> indices;
public:
	SolidSphere();
	SolidSphere(float radius, size_t stacks = 20, size_t slices = 20);
	~SolidSphere();

	vk::VertexInputBindingDescription GetVertexBindingDescription();
	std::vector<vk::VertexInputAttributeDescription> GetVertexAttributeDescription();
	vk::DeviceSize GetVerticesSize() { return sizeof(vertices[0]) * vertices.size(); }
	vk::DeviceSize GetIndiciesSize() { return sizeof(indices[0]) * indices.size(); }
	std::vector<VulkanTools::VertexInput> GetVertices() { return vertices; }
	std::vector<uint16_t> GetIndicies() { return indices; }
};
#endif
