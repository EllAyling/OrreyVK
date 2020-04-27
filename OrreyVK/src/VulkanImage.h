#pragma once
#ifndef VULKANIMAGE_H
#define VULKANIMAGE_H

#include <vulkan/vulkan.hpp>
#include "Types.h"

namespace vko {
	class Image {
	private:
		vk::Device device;
		vk::PhysicalDevice physicalDevice;
	public:
		vk::Image image;
		vk::ImageView imageView;
		vk::DeviceMemory memory;
		vk::DeviceSize size = 0;
		vk::Extent3D extent;
		int mipLevels = 1;

		vk::ImageUsageFlags usageFlags = {};
		vk::MemoryPropertyFlags memoryPropertyFlags = {};
		vk::DescriptorImageInfo descriptor = {};
		vk::Sampler sampler;
		vk::ImageLayout currentLayout = {};

		Image() {};

		Image(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Image image, vk::ImageView imageView, vk::DeviceMemory memory, vk::DeviceSize size, vk::Extent3D extent, int mipLevels, vk::ImageLayout currentLayout, vk::ImageUsageFlags usageFlags, vk::MemoryPropertyFlags memoryPropertyFlags) :
			device(device), physicalDevice(physicalDevice), image(image), imageView(imageView), memory(memory), size(size), extent(extent), mipLevels(mipLevels), currentLayout(currentLayout), usageFlags(usageFlags), memoryPropertyFlags(memoryPropertyFlags)
		{
			vk::SamplerCreateInfo samplerCreateInfo = vk::SamplerCreateInfo();
			samplerCreateInfo.magFilter = vk::Filter::eLinear;
			samplerCreateInfo.minFilter = vk::Filter::eLinear;
			samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
			samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
			samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
			samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
			samplerCreateInfo.mipLodBias = 0.0f;
			samplerCreateInfo.maxAnisotropy = 1.0f;
			samplerCreateInfo.anisotropyEnable = false;
			samplerCreateInfo.compareOp = vk::CompareOp::eNever;
			samplerCreateInfo.minLod = 0.0f;
			samplerCreateInfo.maxLod = float(mipLevels);

			if (mipLevels > 1 && physicalDevice.getFeatures().samplerAnisotropy)
			{
				samplerCreateInfo.maxAnisotropy = physicalDevice.getProperties().limits.maxSamplerAnisotropy;
				samplerCreateInfo.anisotropyEnable = true;
			}

			sampler = device.createSampler(samplerCreateInfo);
			descriptor = vk::DescriptorImageInfo(sampler, imageView, currentLayout);
		}

		void SetDescriptor(vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout layout)
		{
			descriptor = vk::DescriptorImageInfo(sampler, imageView, layout);
		}

		void SetImageLayout(vk::ImageLayout layout)
		{
			currentLayout = layout;
			SetDescriptor(sampler, imageView, layout);
		}

		void Destroy()
		{
			device.destroySampler(sampler);
			device.destroyImageView(imageView);
			device.destroyImage(image);
			device.freeMemory(memory);
		}
	};
}
#endif