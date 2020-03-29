#pragma once
#ifndef VULKANIMAGE_H
#define VULKANIMAGE_H

#include <vulkan/vulkan.hpp>
#include "Types.h"

namespace vko {
	class Image {
	private:
		vk::Device device;
	public:
		vk::Image image;
		vk::ImageView imageView;
		vk::DeviceMemory memory;
		vk::DeviceSize size = 0;
		vk::Extent3D extent;

		vk::ImageUsageFlags usageFlags = {};
		vk::MemoryPropertyFlags memoryPropertyFlags = {};
		vk::DescriptorImageInfo descriptor = {};
		vk::Sampler sampler;
		vk::ImageLayout currentLayout = {};

		Image() {};

		Image(vk::Device device, vk::Image image, vk::ImageView imageView, vk::DeviceMemory memory, vk::DeviceSize size, vk::Extent3D extent, vk::ImageLayout currentLayout, vk::ImageUsageFlags usageFlags, vk::MemoryPropertyFlags memoryPropertyFlags) :
			device(device), image(image), imageView(imageView), memory(memory), size(size), extent(extent), currentLayout(currentLayout), usageFlags(usageFlags), memoryPropertyFlags(memoryPropertyFlags)
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
			samplerCreateInfo.maxLod = 1.0f;

			sampler = device.createSampler(samplerCreateInfo);
			descriptor = vk::DescriptorImageInfo(sampler, imageView, currentLayout);
		}

		void SetDescriptor(vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout layout)
		{
			descriptor = vk::DescriptorImageInfo(sampler, imageView, layout);
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