#include "OrreyVk.h"

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
}

void OrreyVk::Init() {
	InitVulkan(m_window);

	m_sphere = SolidSphere(0.5, 20, 20);
	vko::Buffer vertexStagingBuffer = CreateBuffer(m_sphere.GetVerticesSize(), vk::BufferUsageFlagBits::eTransferSrc, m_sphere.GetVertices().data());
	vko::Buffer indexStagingBuffer = CreateBuffer(m_sphere.GetIndiciesSize(), vk::BufferUsageFlagBits::eTransferSrc, m_sphere.GetIndicies().data());
	m_bufferVertex = CreateBuffer(m_sphere.GetVerticesSize(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, nullptr, vk::MemoryPropertyFlagBits::eDeviceLocal);
	m_bufferIndex = CreateBuffer(m_sphere.GetIndiciesSize(), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, nullptr, vk::MemoryPropertyFlagBits::eDeviceLocal);

	CopyBuffer(vertexStagingBuffer, m_bufferVertex, m_sphere.GetVerticesSize());
	CopyBuffer(indexStagingBuffer, m_bufferIndex, m_sphere.GetIndiciesSize());

	m_graphics.ubo.projection = glm::perspective(glm::radians(45.0f), m_vulkanResources->swapchain.GetDimensions().width / (float)m_vulkanResources->swapchain.GetDimensions().height, 0.1f, 100.0f);
	m_graphics.ubo.projection[1][1] *= -1;
	m_graphics.ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	m_graphics.ubo.model = glm::mat4(1.0f);

	m_graphics.uniformBuffer = CreateBuffer(sizeof(m_graphics.ubo), vk::BufferUsageFlagBits::eUniformBuffer, &m_graphics.ubo);

	vertexStagingBuffer.Destroy();
	indexStagingBuffer.Destroy();

	CreateDescriptorPool();
	CreateDescriptorSetLayout();
	CreateDescriptorSet();

	CreateGraphicsPipelineLayout();
	CreateGraphicsPipeline();

	CreateCommandBuffers();
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
		m_vulkanResources->commandBuffers[i].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
		m_vulkanResources->commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics.pipeline);
		m_vulkanResources->commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphics.pipelineLayout, 0, 1, &m_graphics.descriptorSet, 0, nullptr);
		m_vulkanResources->commandBuffers[i].bindVertexBuffers(0, m_bufferVertex.buffer, { 0 });
		m_vulkanResources->commandBuffers[i].bindIndexBuffer(m_bufferIndex.buffer, { 0 }, vk::IndexType::eUint16);
		m_vulkanResources->commandBuffers[i].drawIndexed(m_sphere.GetIndicies().size(), 1, 0, 0, 0);
		m_vulkanResources->commandBuffers[i].endRenderPass();
		m_vulkanResources->commandBuffers[i].end();
	}
}

void OrreyVk::CreateDescriptorPool()
{
	std::vector<vk::DescriptorPoolSize> poolSizes =
	{
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1)
	};

	vk::DescriptorPoolCreateInfo poolInfo = vk::DescriptorPoolCreateInfo({}, 1, poolSizes.size(), poolSizes.data());
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
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo = vk::PipelineVertexInputStateCreateInfo({}, 1, &(m_sphere.GetVertexBindingDescription()), vertexAttributeDescriptions.size(), vertexAttributeDescriptions.data());

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
	m_vulkanResources->device.waitForFences(m_vulkanResources->fences[m_frameID], true, UINT64_MAX);
	m_vulkanResources->device.resetFences(m_vulkanResources->fences[m_frameID]);

	vk::ResultValue<uint32_t> imageIndex = m_vulkanResources->device.acquireNextImageKHR(m_vulkanResources->swapchain.GetVkObject(), UINT64_MAX, m_vulkanResources->semaphoreImageAquired[m_frameID], {});

	vk::SubmitInfo submitInfo = vk::SubmitInfo();
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_vulkanResources->semaphoreImageAquired[m_frameID];
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_vulkanResources->commandBuffers[imageIndex.value];

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_vulkanResources->semaphoreRender[m_frameID];

	m_vulkanResources->queueGraphics.submit(submitInfo, m_vulkanResources->fences[imageIndex.value]);

	vk::SwapchainKHR swapchains[] = { m_vulkanResources->swapchain.GetVkObject() };
	vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR(1, &m_vulkanResources->semaphoreRender[m_frameID], 1, swapchains, &imageIndex.value);

	m_vulkanResources->queueGraphics.presentKHR(presentInfo);

	m_frameID = (m_frameID + 1) % m_vulkanResources->swapchain.GetImageCount();
}

void OrreyVk::MainLoop() {
	while (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();
		RenderFrame();
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