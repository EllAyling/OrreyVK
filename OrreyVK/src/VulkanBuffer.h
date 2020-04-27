#pragma once
#ifndef VULKANBUFFER_H
#define VULKANBUFFER_H

#include <vulkan/vulkan.hpp>
#include "Types.h"

namespace vko {
	class Buffer {
	private:
		vk::Device device;
	public:
		vk::DeviceMemory memory = {};
		vk::Buffer buffer = {};
		vk::DeviceSize size = 0;
		void* mapped = nullptr;
		vk::BufferUsageFlags usageFlags = {};
		vk::MemoryPropertyFlags memoryPropertyFlags = {};
		vk::DescriptorBufferInfo descriptor = {};

		Buffer() {};

		Buffer(vk::Device device, vk::DeviceMemory memory, vk::Buffer buffer, vk::DeviceSize size, vk::BufferUsageFlags usageFlags, vk::MemoryPropertyFlags memoryPropertyFlags) :
			device(device), memory(memory), buffer(buffer), size(size), usageFlags(usageFlags), memoryPropertyFlags(memoryPropertyFlags) 
		{
			descriptor = vk::DescriptorBufferInfo(buffer, 0, VK_WHOLE_SIZE);
		}

		void SetDescriptor(vk::DeviceSize size, vk::DeviceSize offset)
		{
			descriptor = vk::DescriptorBufferInfo(buffer, offset, size);
		}

		vk::Result Map(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0)
		{
			return device.mapMemory(memory, offset, size, {}, &mapped);
		}

		void UnMap()
		{
			if (mapped)
			{
				device.unmapMemory(memory);
				mapped = nullptr;
			}
		}

		void Destroy()
		{
			UnMap();
			device.destroyBuffer(buffer);
			device.freeMemory(memory);
		}
	};
}
#endif