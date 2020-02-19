#pragma once
#ifndef VULKAN_H
#define VULKAN_H

#include <iostream>

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Vulkan
{
private:
	struct deviceResources
	{
		vk::Device device;
		vk::Instance instance;

		vk::PipelineLayout pipelineLayout;
		vk::Pipeline pipeline;

		vk::DescriptorSetLayout descriptorSetLayout;
		vk::DescriptorSet descriptorSet;

		vk::Semaphore semaphorePresent;
		vk::Semaphore semaphoreRender;

		std::vector<vk::Fence> fences;


	} m_deviceResources;

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

	void CreateInstance();
	void CreateDevice();
	void CreateQueue();

public:
	void Init();
	void Cleanup();
};

#endif // !VULKAN_H