#pragma once
#ifndef VKSWAPCHAIN_H
#define VKSWAPCHAIN_H

#include <vulkan/vulkan.hpp>
#include "Types.h"
namespace vko {
	class VulkanSwapchain
	{
	private:
		vk::Instance instance;
		vk::Device device;
		vk::PhysicalDevice physicalDevice;
		vk::SwapchainKHR swapchain;
		std::vector<VulkanTools::ImageResources> swapchainImages;
		VulkanTools::ImageResources depthImage;
		VulkanTools::ImageResources multiSampleImage;

		uint32_t numOfImages = 0;
		vk::Format swapchainFormat = vk::Format::eUndefined;
		vk::Extent2D dimensions;

	public:

		VulkanSwapchain() {};

		VulkanSwapchain(vk::Instance instance, vk::Device device, vk::PhysicalDevice physicalDevice, vk::SwapchainCreateInfoKHR createInfo)
		{
			this->instance = instance;
			this->device = device;
			this->physicalDevice = physicalDevice;
			this->numOfImages = createInfo.minImageCount;
			this->swapchainFormat = createInfo.imageFormat;
			this->dimensions = createInfo.imageExtent;

			swapchain = this->device.createSwapchainKHR(createInfo);

			std::vector<vk::Image> images;
			images = this->device.getSwapchainImagesKHR(swapchain);

			vk::ImageViewCreateInfo imageViewCreateInfo;
			imageViewCreateInfo.format = swapchainFormat;
			imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
			imageViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;

			for (auto& image : images)
			{
				imageViewCreateInfo.image = image;
				swapchainImages.push_back(VulkanTools::ImageResources(image, this->device.createImageView(imageViewCreateInfo), nullptr));
			}
		}

		~VulkanSwapchain() {};

		std::vector<VulkanTools::ImageResources> GetImages();

		uint32_t GetImageCount() { return numOfImages; }
		vk::Format GetSwapchainFormat() { return swapchainFormat; }
		void SetDepthImage(VulkanTools::ImageResources depthImage) { this->depthImage = depthImage; }
		VulkanTools::ImageResources GetDepthImage() { return depthImage; }
		void SetMultiSampleImage(VulkanTools::ImageResources multiSampleImage) { this->multiSampleImage = multiSampleImage; }
		VulkanTools::ImageResources GetMultiSampleImage() { return multiSampleImage; }
		vk::Extent2D GetDimensions() { return dimensions; }
		vk::SwapchainKHR GetVkObject() { return swapchain; }

		void Destroy();
	};
}
#endif