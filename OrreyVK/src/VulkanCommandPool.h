#pragma once
#ifndef VULKANCOMMANDPOOL_H
#define VULKANCOMMANDPOOL_H

#include <vulkan/vulkan.hpp>

class VulkanCommandPool
{
private:
	vk::CommandPool commandPool;
	vk::Device device;

public:
	VulkanCommandPool() {};
	VulkanCommandPool(vk::Device device, uint32_t queueFamily)
	{
		this->device = device;
		vk::CommandPoolCreateInfo commandPoolInfo = vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queueFamily);
		commandPool = device.createCommandPool(commandPoolInfo);
	}

	vk::CommandBuffer AllocateCommandBuffer();
	std::vector<vk::CommandBuffer> AllocateCommandBuffers(uint32_t count);
	void Destroy();
};

#endif

