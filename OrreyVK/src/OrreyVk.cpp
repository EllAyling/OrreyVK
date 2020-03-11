#include "OrreyVk.h"

#define OBJECTS_PER_GROUP 256

void OrreyVk::Run() {
	InitWindow();
	Init();
	MainLoop();
	Cleanup();
}

void OrreyVk::InitWindow() {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);

	glfwSetKeyCallback(m_window, key_callback);
	glfwSetMouseButtonCallback(m_window, mouse_button_callback);
	glfwSetScrollCallback(m_window, scroll_callback);
}

void OrreyVk::Init() {
	InitVulkan(m_window);

	m_sphere = SolidSphere(0.5, 20, 20);

	//Setup vertex and ubo buffer for graphics
	vko::Buffer vertexStagingBuffer = CreateBuffer(m_sphere.GetVerticesSize(), vk::BufferUsageFlagBits::eTransferSrc, m_sphere.GetVertices().data());
	vko::Buffer indexStagingBuffer = CreateBuffer(m_sphere.GetIndiciesSize(), vk::BufferUsageFlagBits::eTransferSrc, m_sphere.GetIndicies().data());
	m_bufferVertex = CreateBuffer(m_sphere.GetVerticesSize(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, nullptr, vk::MemoryPropertyFlagBits::eDeviceLocal);
	m_bufferIndex = CreateBuffer(m_sphere.GetIndiciesSize(), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, nullptr, vk::MemoryPropertyFlagBits::eDeviceLocal);

	CopyBuffer(vertexStagingBuffer, m_bufferVertex, m_sphere.GetVerticesSize());
	CopyBuffer(indexStagingBuffer, m_bufferIndex, m_sphere.GetIndiciesSize());

	m_graphics.ubo.projection = glm::perspective(glm::radians(60.0f), m_vulkanResources->swapchain.GetDimensions().width / (float)m_vulkanResources->swapchain.GetDimensions().height, 0.1f, 100.0f);
	m_graphics.ubo.view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0, 0.0, -5.0));
	m_graphics.ubo.model = glm::mat4(1.0f);

	m_graphics.uniformBuffer = CreateBuffer(sizeof(m_graphics.ubo), vk::BufferUsageFlagBits::eUniformBuffer, &m_graphics.ubo);
	m_graphics.uniformBuffer.Map();

	vertexStagingBuffer.Destroy();
	indexStagingBuffer.Destroy();

	m_graphics.semaphore = m_vulkanResources->device.createSemaphore(vk::SemaphoreCreateInfo());

	PrepareInstance();

	CreateDescriptorPool();
	CreateDescriptorSetLayout();
	CreateDescriptorSet();

	CreateGraphicsPipelineLayout();
	CreateGraphicsPipeline();
	PrepareCompute();

	CreateCommandBuffers();
}

void OrreyVk::PrepareInstance()
{
	std::vector<CelestialObj> objects;
	for (int i = -256; i < 256; i++)
		objects.push_back({ glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(0.07, 0.0, 0.0, 0.0) });

	uint32_t size = objects.size() * sizeof(CelestialObj);
	vko::Buffer instanceStagingBuffer = CreateBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, objects.data());
	m_bufferInstance = CreateBuffer(size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, nullptr, vk::MemoryPropertyFlagBits::eDeviceLocal);

	CopyBuffer(instanceStagingBuffer, m_bufferInstance, size);
	instanceStagingBuffer.Destroy();

	if (m_queueIDs.graphics.familyID != m_queueIDs.compute.familyID)
	{
		vk::CommandBuffer cmdBuffer = m_vulkanResources->commandPool.AllocateCommandBuffer();
		cmdBuffer.begin(vk::CommandBufferBeginInfo());

		vk::BufferMemoryBarrier barrier = vk::BufferMemoryBarrier(
			vk::AccessFlagBits::eVertexAttributeRead, {},
			m_queueIDs.graphics.familyID, m_queueIDs.compute.familyID,
			m_bufferInstance.buffer, 0, m_bufferInstance.size
		);

		cmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eVertexInput, vk::PipelineStageFlagBits::eComputeShader,
			{}, {}, barrier, {});

		cmdBuffer.end();

		vk::SubmitInfo submitInfo = vk::SubmitInfo();
		vk::Fence fence = m_vulkanResources->device.createFence(vk::FenceCreateInfo());
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer;
		m_vulkanResources->queueGraphics.submit(submitInfo, fence);
		m_vulkanResources->device.waitForFences(fence, true, INT_MAX);
		m_vulkanResources->device.destroyFence(fence);
		m_vulkanResources->commandPool.FreeCommandBuffers(cmdBuffer);
	}
}

void OrreyVk::CreateCommandBuffers()
{
	uint32_t swapchainLength = m_vulkanResources->swapchain.GetImageCount();
	m_vulkanResources->commandBuffers = m_vulkanResources->commandPool.AllocateCommandBuffers(swapchainLength);
	for (size_t i = 0; i < swapchainLength; i++)
	{
		std::vector<vk::ClearValue> clearValues = {};
		clearValues.resize(2);
		clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
		clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0.0f);
		vk::RenderPassBeginInfo renderPassInfo = vk::RenderPassBeginInfo(m_vulkanResources->renderpass, m_vulkanResources->frameBuffers[i]);
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues.data();
		renderPassInfo.renderArea = vk::Rect2D({ 0, 0 }, m_vulkanResources->swapchain.GetDimensions());

		m_vulkanResources->commandBuffers[i].begin(vk::CommandBufferBeginInfo());

		if (m_queueIDs.graphics.familyID != m_queueIDs.compute.familyID)
		{
			vk::BufferMemoryBarrier barrier = vk::BufferMemoryBarrier(
				{}, vk::AccessFlagBits::eVertexAttributeRead,
				m_queueIDs.compute.familyID, m_queueIDs.graphics.familyID,
				m_bufferInstance.buffer, 0, m_bufferInstance.size
			);

			m_vulkanResources->commandBuffers[i].pipelineBarrier(
				vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexInput,
				{}, {}, barrier, {});
		}
			   		 	  
		m_vulkanResources->commandBuffers[i].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
		m_vulkanResources->commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics.pipeline);
		m_vulkanResources->commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphics.pipelineLayout, 0, 1, &m_graphics.descriptorSet, 0, nullptr);
		m_vulkanResources->commandBuffers[i].bindVertexBuffers(0, m_bufferVertex.buffer, { 0 });
		m_vulkanResources->commandBuffers[i].bindVertexBuffers(1, m_bufferInstance.buffer, { 0 });
		m_vulkanResources->commandBuffers[i].bindIndexBuffer(m_bufferIndex.buffer, { 0 }, vk::IndexType::eUint16);
		m_vulkanResources->commandBuffers[i].drawIndexed(m_sphere.GetIndicies().size(), m_bufferInstance.size / sizeof(CelestialObj), 0, 0, 0);
		m_vulkanResources->commandBuffers[i].endRenderPass();

		if (m_queueIDs.graphics.familyID != m_queueIDs.compute.familyID)
		{
			vk::BufferMemoryBarrier barrier = vk::BufferMemoryBarrier(
				vk::AccessFlagBits::eVertexAttributeRead, {},
				m_queueIDs.graphics.familyID, m_queueIDs.compute.familyID,
				m_bufferInstance.buffer, 0, m_bufferInstance.size
			);

			m_vulkanResources->commandBuffers[i].pipelineBarrier(
				vk::PipelineStageFlagBits::eVertexInput, vk::PipelineStageFlagBits::eComputeShader,
				{}, {}, barrier, {});
		}

		m_vulkanResources->commandBuffers[i].end();
	}
}

void OrreyVk::CreateDescriptorPool()
{
	std::vector<vk::DescriptorPoolSize> poolSizes =
	{
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2),
		vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1)
	};

	vk::DescriptorPoolCreateInfo poolInfo = vk::DescriptorPoolCreateInfo({}, 3, poolSizes.size(), poolSizes.data());
	m_vulkanResources->descriptorPool = m_vulkanResources->device.createDescriptorPool(poolInfo);
}

void OrreyVk::CreateDescriptorSetLayout()
{
	std::vector<vk::DescriptorSetLayoutBinding> layoutBindings =
	{
		vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex)
	};

	m_graphics.descriptorSetLayout = m_vulkanResources->device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, layoutBindings.size(), layoutBindings.data()));
}

void OrreyVk::CreateDescriptorSet()
{
	vk::DescriptorSetAllocateInfo allocInfo = vk::DescriptorSetAllocateInfo(m_vulkanResources->descriptorPool, 1, &m_graphics.descriptorSetLayout);
	m_graphics.descriptorSet = m_vulkanResources->device.allocateDescriptorSets(allocInfo)[0];

	std::vector<vk::WriteDescriptorSet> writeSets =
	{
		vk::WriteDescriptorSet(m_graphics.descriptorSet, 2, 0, 1, vk::DescriptorType::eUniformBuffer, {}, &(m_graphics.uniformBuffer.descriptor))
	};

	m_vulkanResources->device.updateDescriptorSets(writeSets.size(), writeSets.data(), 0, nullptr);
}

void OrreyVk::CreateGraphicsPipelineLayout()
{
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo = vk::PipelineLayoutCreateInfo({}, 1, &m_graphics.descriptorSetLayout);
	m_graphics.pipelineLayout = m_vulkanResources->device.createPipelineLayout(pipelineLayoutInfo);
}

void OrreyVk::CreateGraphicsPipeline()
{
	vk::ShaderModule vertShader = CompileShader("resources/shaders/shader.vert", shaderc_shader_kind::shaderc_glsl_vertex_shader);
	vk::ShaderModule fragShader = CompileShader("resources/shaders/shader.frag", shaderc_shader_kind::shaderc_glsl_fragment_shader);

	vk::PipelineShaderStageCreateInfo vertShaderStage = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertShader, "main");
	vk::PipelineShaderStageCreateInfo fragShaderStage = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragShader, "main");
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStage, fragShaderStage };

	std::vector<vk::VertexInputAttributeDescription> vertexAttributeDescriptions = m_sphere.GetVertexAttributeDescription();
	vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription(2, 1, vk::Format::eR32G32B32A32Sfloat, offsetof(CelestialObj, position)));

	std::vector<vk::VertexInputBindingDescription> bindingDesc = { m_sphere.GetVertexBindingDescription() };
	bindingDesc.push_back(vk::VertexInputBindingDescription(1, sizeof(CelestialObj), vk::VertexInputRate::eInstance));

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo = vk::PipelineVertexInputStateCreateInfo({}, 2, bindingDesc.data(), vertexAttributeDescriptions.size(), vertexAttributeDescriptions.data());

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly = vk::PipelineInputAssemblyStateCreateInfo({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

	vk::Viewport viewport = vk::Viewport(0.0, 0.0, m_vulkanResources->swapchain.GetDimensions().width, m_vulkanResources->swapchain.GetDimensions().height, 0.0, 1.0);
	vk::Rect2D scissor = vk::Rect2D({ 0,0 }, m_vulkanResources->swapchain.GetDimensions());
	vk::PipelineViewportStateCreateInfo viewPortState = vk::PipelineViewportStateCreateInfo({}, 1, &viewport, 1, &scissor);

	vk::PipelineRasterizationStateCreateInfo rastierizer = vk::PipelineRasterizationStateCreateInfo();
	rastierizer.cullMode = vk::CullModeFlagBits::eBack;
	rastierizer.frontFace = vk::FrontFace::eCounterClockwise;

	vk::PipelineMultisampleStateCreateInfo msState = vk::PipelineMultisampleStateCreateInfo();

	vk::PipelineDepthStencilStateCreateInfo depthStencilInfo = vk::PipelineDepthStencilStateCreateInfo({}, VK_TRUE, VK_TRUE, vk::CompareOp::eLess);

	vk::PipelineColorBlendAttachmentState colourBlendAttachState = vk::PipelineColorBlendAttachmentState();
	colourBlendAttachState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	vk::PipelineColorBlendStateCreateInfo colourBlendInfo = vk::PipelineColorBlendStateCreateInfo();
	colourBlendInfo.pAttachments = &colourBlendAttachState;
	colourBlendInfo.attachmentCount = 1;

	//vk::PipelineDynamicStateCreateInfo dynamicState = vk::PipelineDynamicStateCreateInfo();

	vk::GraphicsPipelineCreateInfo pipelineInfo = vk::GraphicsPipelineCreateInfo();
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;

	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewPortState;
	pipelineInfo.pRasterizationState = &rastierizer;
	pipelineInfo.pMultisampleState = &msState;
	pipelineInfo.pDepthStencilState = &depthStencilInfo;
	pipelineInfo.pColorBlendState = &colourBlendInfo;
	pipelineInfo.pDynamicState = nullptr;

	pipelineInfo.layout = m_graphics.pipelineLayout;
	pipelineInfo.renderPass = m_vulkanResources->renderpass;
	pipelineInfo.subpass = 0;

	m_graphics.pipeline = m_vulkanResources->device.createGraphicsPipeline(nullptr, pipelineInfo);

	m_vulkanResources->device.destroyShaderModule(vertShader);
	m_vulkanResources->device.destroyShaderModule(fragShader);
}

void OrreyVk::RenderFrame()
{
	m_vulkanResources->device.waitForFences(m_compute.fence, true, UINT64_MAX);
	m_vulkanResources->device.resetFences(m_compute.fence);
	m_vulkanResources->device.waitForFences(m_vulkanResources->fences[m_frameID], true, UINT64_MAX);
	m_vulkanResources->device.resetFences(m_vulkanResources->fences[m_frameID]);

	vk::ResultValue<uint32_t> imageIndex = m_vulkanResources->device.acquireNextImageKHR(m_vulkanResources->swapchain.GetVkObject(), UINT64_MAX, m_vulkanResources->semaphoreImageAquired[m_frameID], {});

	vk::Semaphore waitSemaphores[] = { m_compute.semaphore, m_vulkanResources->semaphoreImageAquired[m_frameID]  };
	vk::Semaphore signalSemaphores[] = { m_graphics.semaphore, m_vulkanResources->semaphoreRender[m_frameID] };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eVertexInput, vk::PipelineStageFlagBits::eColorAttachmentOutput };

	vk::SubmitInfo submitInfo = vk::SubmitInfo();
	submitInfo.waitSemaphoreCount = 2;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.signalSemaphoreCount = 2;
	submitInfo.pSignalSemaphores = signalSemaphores;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_vulkanResources->commandBuffers[imageIndex.value];

	m_vulkanResources->queueGraphics.submit(submitInfo, m_vulkanResources->fences[imageIndex.value]);

	vk::SubmitInfo computeSubmitInfo = vk::SubmitInfo();
	computeSubmitInfo.waitSemaphoreCount = 1;
	computeSubmitInfo.pWaitSemaphores = &m_graphics.semaphore;
	vk::PipelineStageFlags computeWaitStages[] = { vk::PipelineStageFlagBits::eComputeShader };
	computeSubmitInfo.pWaitDstStageMask = computeWaitStages;
	computeSubmitInfo.signalSemaphoreCount = 1;
	computeSubmitInfo.pSignalSemaphores = &m_compute.semaphore;
	computeSubmitInfo.commandBufferCount = 1;
	computeSubmitInfo.pCommandBuffers = &m_compute.cmdBuffer;

	m_vulkanResources->queueCompute.submit(computeSubmitInfo, m_compute.fence);

	vk::SwapchainKHR swapchains[] = { m_vulkanResources->swapchain.GetVkObject() };
	vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR(1, &m_vulkanResources->semaphoreRender[m_frameID], 1, swapchains, &imageIndex.value);

	m_vulkanResources->queueGraphics.presentKHR(presentInfo);

	m_frameID = (m_frameID + 1) % m_vulkanResources->swapchain.GetImageCount();
}

void OrreyVk::UpdateCamera(float xPos, float yPos, float deltaTime)
{
	float dx = m_camera.mousePos.x - xPos;
	float dy = m_camera.mousePos.y - yPos;

	m_camera.rotation.x += dy * m_camera.rotationSpeed * deltaTime;
	m_camera.rotation.y -= dx * m_camera.rotationSpeed * deltaTime;

	m_camera.mousePos = glm::vec2(xPos, yPos);
}

void OrreyVk::UpdateCameraUniformBuffer()
{
	m_graphics.ubo.view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0, 0.0, m_camera.zoom));
	m_graphics.ubo.view = glm::rotate(m_graphics.ubo.view, glm::radians(m_camera.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	m_graphics.ubo.view = glm::rotate(m_graphics.ubo.view, glm::radians(m_camera.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	m_graphics.ubo.view = glm::rotate(m_graphics.ubo.view, glm::radians(m_camera.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	memcpy(m_graphics.uniformBuffer.mapped, &m_graphics.ubo, sizeof(m_graphics.ubo));

	m_camera.viewUpdated = false;
}

void OrreyVk::UpdateComputeUniformBuffer()
{
	m_compute.ubo.deltaT = m_frameTime;
	memcpy(m_compute.uniformBuffer.mapped, &m_compute.ubo, sizeof(m_compute.ubo));
}

void OrreyVk::PrepareCompute()
{
	//Compute Uniform buffer
	m_compute.uniformBuffer = CreateBuffer(sizeof(m_compute.ubo), vk::BufferUsageFlagBits::eUniformBuffer);
	m_compute.ubo.objectCount = m_bufferInstance.size / sizeof(CelestialObj);
	m_compute.uniformBuffer.Map();
	UpdateComputeUniformBuffer();

	std::vector<vk::DescriptorSetLayoutBinding> descSetLayoutBindings =
	{
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute),
		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute)
	};

	m_compute.descriptorSetLayout = m_vulkanResources->device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, descSetLayoutBindings.size(), descSetLayoutBindings.data()));

	m_compute.pipelineLayout = m_vulkanResources->device.createPipelineLayout(vk::PipelineLayoutCreateInfo({}, 1, &m_compute.descriptorSetLayout));

	vk::DescriptorSetAllocateInfo allocInfo = vk::DescriptorSetAllocateInfo(m_vulkanResources->descriptorPool, 1, &m_compute.descriptorSetLayout);
	m_compute.descriptorSet = m_vulkanResources->device.allocateDescriptorSets(allocInfo)[0];

	std::vector<vk::WriteDescriptorSet> writeSets =
	{
		vk::WriteDescriptorSet(m_compute.descriptorSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, {}, &(m_bufferInstance.descriptor)),
		vk::WriteDescriptorSet(m_compute.descriptorSet, 1, 0, 1, vk::DescriptorType::eUniformBuffer, {}, &(m_compute.uniformBuffer.descriptor))
	};

	m_vulkanResources->device.updateDescriptorSets(writeSets.size(), writeSets.data(), 0, nullptr);

	vk::ComputePipelineCreateInfo pipelineCreateInfo = vk::ComputePipelineCreateInfo();
	pipelineCreateInfo.layout = m_compute.pipelineLayout;
	vk::ShaderModule computeShader = CompileShader("resources/shaders/shader.comp", shaderc_shader_kind::shaderc_glsl_compute_shader);
	vk::PipelineShaderStageCreateInfo computeShaderStage = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eCompute, computeShader, "main");
	pipelineCreateInfo.stage = computeShaderStage;
	m_compute.pipeline = m_vulkanResources->device.createComputePipeline(nullptr, pipelineCreateInfo);

	m_compute.commandPool = vko::VulkanCommandPool(m_vulkanResources->device, m_queueIDs.compute.familyID, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	m_compute.cmdBuffer = m_compute.commandPool.AllocateCommandBuffer();

	m_compute.fence = m_vulkanResources->device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
	m_compute.semaphore = m_vulkanResources->device.createSemaphore(vk::SemaphoreCreateInfo());
	vk::SubmitInfo submitInfo = vk::SubmitInfo();
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_compute.semaphore;
	m_vulkanResources->queueCompute.submit(submitInfo, nullptr);
	m_vulkanResources->queueCompute.waitIdle();


	if (m_queueIDs.graphics.familyID != m_queueIDs.compute.familyID)
	{
		vk::CommandBuffer cmdBuffer = m_compute.commandPool.AllocateCommandBuffer();
		cmdBuffer.begin(vk::CommandBufferBeginInfo());

		vk::BufferMemoryBarrier barrier = vk::BufferMemoryBarrier(
			{}, vk::AccessFlagBits::eShaderWrite,
			m_queueIDs.graphics.familyID, m_queueIDs.compute.familyID,
			m_bufferInstance.buffer, 0, m_bufferInstance.size
		);

		cmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eVertexInput, vk::PipelineStageFlagBits::eComputeShader,
			{}, {}, barrier, {});

		barrier = vk::BufferMemoryBarrier(
			vk::AccessFlagBits::eShaderWrite, {},
			m_queueIDs.compute.familyID, m_queueIDs.graphics.familyID,
			m_bufferInstance.buffer, 0, m_bufferInstance.size
		);

		cmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexInput,
			{}, {}, barrier, {});

		cmdBuffer.end();

		vk::SubmitInfo submitInfo = vk::SubmitInfo();
		vk::Fence fence = m_vulkanResources->device.createFence(vk::FenceCreateInfo());
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer;
		m_vulkanResources->queueCompute.submit(submitInfo, fence);
		m_vulkanResources->device.waitForFences(fence, true, INT_MAX);
		m_vulkanResources->device.destroyFence(fence);
		m_compute.commandPool.FreeCommandBuffers(cmdBuffer);
	}

	m_compute.cmdBuffer.begin(vk::CommandBufferBeginInfo());
	if (m_queueIDs.graphics.familyID != m_queueIDs.compute.familyID)
	{
		vk::BufferMemoryBarrier barrier = vk::BufferMemoryBarrier(
			{}, vk::AccessFlagBits::eShaderWrite,
			m_queueIDs.graphics.familyID, m_queueIDs.compute.familyID,
			m_bufferInstance.buffer, 0, m_bufferInstance.size
		);

		m_compute.cmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eVertexInput, vk::PipelineStageFlagBits::eComputeShader,
			{}, {}, barrier, {});
	}

	m_compute.cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute.pipeline);
	m_compute.cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_compute.pipelineLayout, 0, m_compute.descriptorSet, {});
	m_compute.cmdBuffer.dispatch(ceil((m_bufferInstance.size / sizeof(CelestialObj)) / OBJECTS_PER_GROUP), 1, 1);

	if (m_queueIDs.graphics.familyID != m_queueIDs.compute.familyID)
	{
		vk::BufferMemoryBarrier barrier = vk::BufferMemoryBarrier(
			vk::AccessFlagBits::eShaderWrite, {},
			m_queueIDs.compute.familyID, m_queueIDs.graphics.familyID,
			m_bufferInstance.buffer, 0, m_bufferInstance.size
		);

		m_compute.cmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexInput,
			{}, {}, barrier, {});
	}

	m_compute.cmdBuffer.end();
}

void OrreyVk::MainLoop() {
	while (!glfwWindowShouldClose(m_window)) {
		auto tStart = std::chrono::high_resolution_clock::now();

		glfwPollEvents();
		RenderFrame();

		double xPos, yPos;
		glfwGetCursorPos(m_window, &xPos, &yPos);

		if (glfwGetInputMode(m_window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
		{
			if (m_camera.mousePos.x != xPos || m_camera.mousePos.y != yPos)
			{
				m_camera.viewUpdated = true;
				UpdateCamera(xPos, yPos, m_frameTime);
			}
		}

		if (m_camera.viewUpdated)
			UpdateCameraUniformBuffer();

		UpdateComputeUniformBuffer();

		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		m_frameTime = (float)tDiff / 1000.0f;
	}
}

void OrreyVk::Cleanup() {
	Vulkan::Cleanup();

	m_bufferVertex.Destroy();
	m_bufferIndex.Destroy();
	m_graphics.uniformBuffer.Destroy();

	m_vulkanResources->device.destroyDescriptorSetLayout(m_graphics.descriptorSetLayout);
	m_vulkanResources->device.destroyDescriptorPool(m_vulkanResources->descriptorPool);
	
	m_vulkanResources->device.destroyPipeline(m_graphics.pipeline);
	m_vulkanResources->device.destroyPipelineLayout(m_graphics.pipelineLayout);

	glfwDestroyWindow(m_window);

	glfwTerminate();
}