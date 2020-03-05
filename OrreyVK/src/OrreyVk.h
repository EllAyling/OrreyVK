#pragma once

#ifndef ORREYVK_H
#define ORREYVK_H

#include <GLFW/glfw3.h>
#include "Vulkan.h"

class OrreyVk : Vulkan {
public:
	void Run();
	void InitWindow();
	void Init();
	void MainLoop();
	void Cleanup();

	OrreyVk() {};

private:
	GLFWwindow * m_window;

	SolidSphere m_sphere;

	struct {
		struct {
			glm::mat4 projection;
			glm::mat4 model;
			glm::mat4 view;
		} ubo;

		vko::Buffer uniformBuffer;
		vk::DeviceMemory uniformBufferMemory;
		vk::DescriptorSetLayout descriptorSetLayout;
		vk::DescriptorSet descriptorSet;
		vk::PipelineLayout pipelineLayout;
		vk::Pipeline pipeline;
	} m_graphics;

	struct {
		vko::Buffer storageBuffer;
		vko::Buffer uniformBuffer;
		vko::VulkanCommandPool commandPool;
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

	vko::Buffer m_bufferVertex;
	vko::Buffer m_bufferIndex;

	void CreateCommandBuffers();
	void CreateDescriptorPool();
	void CreateDescriptorSetLayout();
	void CreateDescriptorSet();
	void CreateGraphicsPipelineLayout();
	void CreateGraphicsPipeline();

	void RenderFrame();
};
#endif // !ORREYVK_H