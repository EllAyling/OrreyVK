#pragma once
#ifndef VULKAN_H
#define VULKAN_H
#include <fstream>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <shaderc/shaderc.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb/stb_image.h>

#include "VulkanSwapchain.h"
#include "VulkanCommandPool.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "Types.h"
#include "SolidSphere.h"

class Vulkan
{
protected:
	struct VulkanResources
	{
		vk::Device device;
		vk::Instance instance;
		vk::PhysicalDevice physicalDevice;
		vk::SurfaceKHR surface;
		vko::VulkanSwapchain swapchain;
		GLFWwindow* window;
		vk::Queue queueGraphics;
		vk::Queue queueCompute;
		vk::Queue queueTransfer;

		vk::RenderPass renderpass;
		std::vector<vk::Framebuffer> frameBuffers;
		vko::VulkanCommandPool commandPool;
		std::vector<vk::CommandBuffer> commandBuffers;
		
		vko::VulkanCommandPool commandPoolTransfer;

		vk::DescriptorPool descriptorPool;

		std::vector <vk::Semaphore> semaphoreImageAquired;
		std::vector <vk::Semaphore> semaphoreRender;
		std::vector<vk::Fence> fences;

		~VulkanResources() {}
	};

	std::unique_ptr<VulkanResources> m_vulkanResources;
	VulkanTools::QueueFamilies m_queueIDs;
	uint32_t m_frameID = 0;

	uint32_t GetMemoryTypeIndex(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

	void CreateInstance(VulkanTools::InstanceExtenstions extensionsRequested = VulkanTools::InstanceExtenstions());
	void CreateSurface(GLFWwindow* window);
	void CreateDevice(VulkanTools::DeviceExtensions extensionsRequested = VulkanTools::DeviceExtensions());
	void CreateSwapchain();
	void CreateRenderpass();
	void CreateFramebuffers();

	void CreateCommandPool();
	void CreateFencesAndSemaphores();
public:
	void InitVulkan(GLFWwindow* window);
	void Cleanup();

	vk::DeviceMemory AllocateAndBindMemory(vk::Image image, vk::MemoryPropertyFlags memoryFlags = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryAllocateInfo *allocInfoOut = nullptr);
	vk::DeviceMemory AllocateAndBindMemory(vk::Buffer buffer, vk::MemoryPropertyFlags memoryFlags = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryAllocateInfo *allocInfoOut = nullptr);

	vko::Image CreateImage(vk::ImageType imageType, vk::Format format, vk::Extent3D extent, vk::ImageUsageFlags usage, vk::ImageAspectFlagBits aspectFlags,
		vk::ImageCreateFlags flags = {}, vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
		vk::MemoryPropertyFlags memoryFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
		int layerCount = 1, vk::SharingMode sharingMode = vk::SharingMode::eExclusive, vk::ImageTiling tiling = vk::ImageTiling::eOptimal);

	vko::Buffer CreateBuffer(uint32_t size, vk::BufferUsageFlags usage, const void* data = nullptr, vk::MemoryPropertyFlags memoryFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
							vk::SharingMode sharingMode = vk::SharingMode::eExclusive);

	void CopyBuffer(vko::Buffer srcBuffer, vko::Buffer dstBuffer, vk::DeviceSize size);

	vk::ShaderModule CompileShader(const std::string& fileName, shaderc_shader_kind type);

	vko::Image CreateTexture(vk::ImageType imageType, vk::Format format, const char* filePath, vk::ImageUsageFlags usage);
	vko::Image Create2DTextureArray(vk::Format format, std::vector<const char*> filePaths, vk::ImageUsageFlags usage);
};

#endif // !VULKAN_H