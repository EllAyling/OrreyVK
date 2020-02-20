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

		vk::RenderPass renderpass;
		std::vector<vk::Framebuffer> frameBuffers;

		vk::PipelineLayout pipelineLayout;
		vk::Pipeline pipeline;

		VulkanCommandPool commandPool;
		std::vector<vk::CommandBuffer> commandBuffers;

		vk::DescriptorSetLayout descriptorSetLayout;
		vk::DescriptorSet descriptorSet;

		std::vector <vk::Semaphore> semaphoreImageAquired;
		std::vector <vk::Semaphore> semaphoreRender;
		std::vector<vk::Fence> fences;

		~VulkanResources() {}
	};

	std::unique_ptr<VulkanResources> m_vulkanResources;
	VulkanTools::QueueFamilies m_queueIDs;
	uint32_t m_frameID = 0;

	//Vertex layout
	struct Vertex {
		float position[3];
		float color[3];
	};

	//Vertex buffer
	struct {
		vk::DeviceMemory memory;
		vk::Buffer buffer;
	} m_vertices;

	// Index buffer
	struct
	{
		vk::DeviceMemory memory;
		vk::Buffer buffer;
		uint32_t count;
	} m_indices;

	// Uniform buffer block object
	struct {
		vk::DeviceMemory memory;
		vk::Buffer buffer;
		vk::DescriptorBufferInfo descriptor;
	}  m_uniformBufferVS;

	struct {
		glm::mat4 projectionMatrix;
		glm::mat4 modelMatrix;
		glm::mat4 viewMatrix;
	} uboVS;

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

public:
	void Init(GLFWwindow* window);
	void RenderFrame();
	void Cleanup();

	VulkanTools::ImagePair CreateImage(vk::ImageType imageType, vk::Format format, vk::Extent3D extent, vk::ImageUsageFlagBits usage, vk::ImageAspectFlagBits aspectFlags, vk::DeviceMemory& memoryOut,
		vk::ImageCreateFlagBits flags = {}, vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
		vk::MemoryPropertyFlagBits memoryFlags = vk::MemoryPropertyFlagBits::eDeviceLocal, vk::SharingMode sharingMode = vk::SharingMode::eExclusive,
		vk::ImageTiling tiling = vk::ImageTiling::eOptimal);

	vk::ShaderModule CompileShader(const std::string& fileName, shaderc_shader_kind type);
};

#endif // !VULKAN_H