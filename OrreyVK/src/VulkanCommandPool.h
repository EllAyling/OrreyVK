#pragma once
#ifndef VULKANCOMMANDPOOL_H
#define VULKANCOMMANDPOOL_H

#include <vulkan/vulkan.hpp>

namespace vko {
	class VulkanCommandPool
	{
	private:
		vk::CommandPool commandPool;
		vk::Device device;

	public:
		VulkanCommandPool() {};
		VulkanCommandPool(vk::Device device, uint32_t queueFamily, vk::CommandPoolCreateFlagBits flags)
		{
			this->device = device;
			vk::CommandPoolCreateInfo commandPoolInfo = vk::CommandPoolCreateInfo(flags, queueFamily);
			commandPool = device.createCommandPool(commandPoolInfo);
		}

		vk::CommandBuffer AllocateCommandBuffer();
		std::vector<vk::CommandBuffer> AllocateCommandBuffers(uint32_t count);
		void FreeCommandBuffers(vk::ArrayProxy<const vk::CommandBuffer> commandBuffers);
		void Destroy();
	};
}
#endif

