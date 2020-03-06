#pragma once

#ifndef ORREYVK_H
#define ORREYVK_H

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include "Vulkan.h"

class OrreyVk : Vulkan {
public:
	void Run();
	void InitWindow();
	void Init();
	void MainLoop();
	void Cleanup();
	void UpdateCamera(float xPos, float yPos, float deltaTime);
	
	struct {
		glm::vec2 mousePos = glm::vec2();
		float zoom = -5.0f;
		float rotationSpeed = 7.5f;
		float movementSpeed = 1.0f;
		bool viewUpdated = false;

		glm::vec3 rotation = glm::vec3();
		glm::vec3 position = glm::vec3();

		struct {
			bool up = false;
			bool down = false;
			bool left = false;
			bool right = false;
		} keys;
	} m_camera;

	OrreyVk() {};

private:
	GLFWwindow * m_window;
	SolidSphere m_sphere;

	float m_frameTime = 1.0f;
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
	vko::Buffer m_bufferInstance;

	void CreateCommandBuffers();
	void CreateDescriptorPool();
	void CreateDescriptorSetLayout();
	void CreateDescriptorSet();
	void CreateGraphicsPipelineLayout();
	void CreateGraphicsPipeline();

	void RenderFrame();

	void PrepareInstance();
	void UpdateUniformBuffer();

};
	void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
	void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
	void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
#endif // !ORREYVK_H