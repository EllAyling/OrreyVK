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

#include "VulkanSwapchain.h"
#include "VulkanCommandPool.h"
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
		VulkanSwapchain swapchain;
		GLFWwindow* window;
		vk::Queue queueGraphics;
		vk::Queue queueCompute;
		vk::Queue queueTransfer;
		
		VulkanCommandPool commandPoolTransfer;

		vk::DescriptorPool descriptorPool;

		std::vector <vk::Semaphore> semaphoreImageAquired;
		std::vector <vk::Semaphore> semaphoreRender;
		std::vector<vk::Fence> fences;

		~VulkanResources() {}
	};

	std::unique_ptr<VulkanResources> m_vulkanResources;
	VulkanTools::QueueFamilies m_queueIDs;
	uint32_t m_frameID = 0;
	SolidSphere m_sphere;

	struct {
		struct {
			glm::mat4 projection;
			glm::mat4 model;
			glm::mat4 view;
		} ubo;

		vk::Buffer uniformBuffer;
		vk::DeviceMemory uniformBufferMemory;
		vk::DescriptorSetLayout descriptorSetLayout;
		vk::DescriptorSet descriptorSet;
		vk::PipelineLayout pipelineLayout;
		vk::Pipeline pipeline;

		VulkanCommandPool commandPool;
		std::vector<vk::CommandBuffer> commandBuffers;
		vk::RenderPass renderpass;
		std::vector<vk::Framebuffer> frameBuffers;

	} m_graphics;

	struct {
		vk::Buffer storageBuffer;
		vk::Buffer uniformBuffer;
		VulkanCommandPool commandPool;
		vk::CommandBuffer cmdBuffer;
		vk::DescriptorSetLayout descriptorSetLayout;
		vk::DescriptorSet descriptorSet;
		vk::PipelineLayout pipelineLayout;
		vk::Pipeline pipeline;
		struct computeUbo {
			float deltaT;
			int32_t planetCount;
		} ubo;
	} m_compute;

	//Vertex buffer
	struct {
		vk::DeviceMemory memory;
		vk::Buffer buffer;
	} m_bufferVertex;

	// Index buffer
	struct
	{
		vk::DeviceMemory memory;
		vk::Buffer buffer;
		uint32_t count;
	} m_bufferIndex;

	// Uniform buffer block object
	struct {
		vk::DeviceMemory memory;
		vk::Buffer buffer;
		vk::DescriptorBufferInfo descriptor;
	}  m_uniformBufferVS;

	uint32_t GetMemoryTypeIndex(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

	void CreateInstance(VulkanTools::InstanceExtenstions extensionsRequested = VulkanTools::InstanceExtenstions());
	void CreateSurface(GLFWwindow* window);
	void CreateDevice(VulkanTools::DeviceExtensions extensionsRequested = VulkanTools::DeviceExtensions());
	void CreateSwapchain();
	void CreateRenderpass();
	void CreateFramebuffers();
	void CreateGraphicsPipelineLayout();
	void CreateGraphicsPipeline();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateFencesAndSemaphores();
	void CreateDescriptorPool();
	void CreateDescriptorSetLayout();
	void CreateDescriptorSet();

public:
	void Init(GLFWwindow* window);
	void RenderFrame();
	void Cleanup();

	vk::DeviceMemory AllocateAndBindMemory(vk::Image image, vk::MemoryPropertyFlags memoryFlags = vk::MemoryPropertyFlagBits::eDeviceLocal);
	vk::DeviceMemory AllocateAndBindMemory(vk::Buffer buffer, vk::MemoryPropertyFlags memoryFlags = vk::MemoryPropertyFlagBits::eDeviceLocal);

	VulkanTools::ImagePair CreateImage(vk::ImageType imageType, vk::Format format, vk::Extent3D extent, vk::ImageUsageFlagBits usage, vk::ImageAspectFlagBits aspectFlags, vk::DeviceMemory* memoryOut = nullptr,
		vk::ImageCreateFlagBits flags = {}, vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
		vk::MemoryPropertyFlags memoryFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
		vk::SharingMode sharingMode = vk::SharingMode::eExclusive, vk::ImageTiling tiling = vk::ImageTiling::eOptimal);

	vk::Buffer CreateBuffer(uint32_t size, vk::BufferUsageFlags usage, vk::DeviceMemory* memoryOut = nullptr, const void* data = nullptr, vk::MemoryPropertyFlags memoryFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
							vk::SharingMode sharingMode = vk::SharingMode::eExclusive);

	void CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

	vk::ShaderModule CompileShader(const std::string& fileName, shaderc_shader_kind type);
};

#endif // !VULKAN_H