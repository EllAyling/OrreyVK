#include "OrreyVk.h"

#define OBJECTS_PER_GROUP 256
#define OBJECTS_TO_SPAWN 102400
#define SCALE 10

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

	m_window = glfwCreateWindow(1980, 1080, "Vulkan", nullptr, nullptr);

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

	m_graphics.ubo.projection = glm::perspective(glm::radians(60.0f), m_vulkanResources->swapchain.GetDimensions().width / (float)m_vulkanResources->swapchain.GetDimensions().height, 0.1f, std::numeric_limits<float>::max());

	m_graphics.ubo.view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0, 0.0, m_camera.zoom));
	m_camera.rotation.x = -90.0;
	m_camera.rotation.y = 0.0;
	m_camera.rotation.z = 0.0;
	m_graphics.ubo.view = glm::rotate(m_graphics.ubo.view, glm::radians(m_camera.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	m_graphics.ubo.view = glm::rotate(m_graphics.ubo.view, glm::radians(m_camera.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	m_graphics.ubo.view = glm::rotate(m_graphics.ubo.view, glm::radians(m_camera.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	m_graphics.ubo.model = glm::mat4(1.0f);

	m_graphics.uniformBuffer = CreateBuffer(sizeof(m_graphics.ubo), vk::BufferUsageFlagBits::eUniformBuffer, &m_graphics.ubo);
	m_graphics.uniformBuffer.Map();
	memcpy(m_graphics.uniformBuffer.mapped, &m_graphics.ubo, sizeof(m_graphics.ubo));

	vertexStagingBuffer.Destroy();
	indexStagingBuffer.Destroy();

	m_graphics.semaphore = m_vulkanResources->device.createSemaphore(vk::SemaphoreCreateInfo());

	//https://www.solarsystemscope.com/textures/
	std::vector<const char*> paths = { 
		"resources/sun.jpg", 
		"resources/mercury.jpg",
		"resources/venus.jpg",
		"resources/earth.jpg",
		"resources/mars.jpg",
		"resources/jupiter.jpg",
		"resources/saturn.jpg",
		"resources/uranus.jpg",
		"resources/neptune.jpg"
	};
	m_textureArrayPlanets = Create2DTextureArray(vk::Format::eR8G8B8A8Unorm, paths, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
	
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
	std::default_random_engine rndGenerator((unsigned)time(nullptr));
	std::uniform_real_distribution<float> uniformDist(0.0, 1.0);

	int objectsToSpawn = OBJECTS_TO_SPAWN;
	while (objectsToSpawn % OBJECTS_PER_GROUP != 0)
		objectsToSpawn--;

	//Converted G constant for AU/SM, T: 1s ~= 1 earth sidereal day
	double G = 0.0002959122083;
	auto initialVelocity = [G](float r) { return sqrt(G / r);  };
	auto degToRad = [](float deg) {return deg * M_PI / 180; };

	float orbitalPeriod = sqrt((4 * M_PI * M_PI * 1.0) / G);

	objects.resize(objectsToSpawn);
	objects[0] = { glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0, 0.0, 0.0, 0.0), glm::vec4(5, 5, 5, 0), glm::vec4(degToRad(-7.25), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(15.228), 0.0, 0.0) }; //Sun
	//				Distance in AU						 Mass in Solar Mass,				Velocity in AU					Scale as ratio of Earth			Texture index   Rotation in degrees					Rotation Speed 
	objects[1] = { glm::vec4(0.39	* SCALE, 0.0f, 0.0f, 1.651502e-7),	glm::vec4(0.0, 0.0,	initialVelocity(0.39),	0.0),	glm::vec4(0.0553, 0.0553, 0.0553,	1), glm::vec4(degToRad(-0.01), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(6.12), 0.0, 0.0) };		//Mercury
	objects[2] = { glm::vec4(0.723	* SCALE, 0.0f, 0.0f, 2.447225e-6),	glm::vec4(0.0, 0.0, initialVelocity(0.723), 0.0),	glm::vec4(0.949, 0.949, 0.949,		2), glm::vec4(degToRad(-117.4), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(-1.476), 0.0, 0.0) };		//Venus
	objects[3] = { glm::vec4(1.0	* SCALE, 0.0f, 0.0f, 3.0027e-6),	glm::vec4(0.0, 0.0, initialVelocity(1.0),	0.0),	glm::vec4(1.0, 1.0, 1.0,			3), glm::vec4(degToRad(-23.5), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(360), 0.0, 0.0) };		//Earth
	objects[4] = { glm::vec4(1.524	* SCALE, 0.0f, 0.0f, 3.212921e-7),	glm::vec4(0.0, 0.0, initialVelocity(1.524), 0.0),	glm::vec4(0.532, 0.532, 0.532,		4), glm::vec4(degToRad(-25.19), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(345.6), 0.0, 0.0) };		//Mars
	objects[5] = { glm::vec4(5.2	* SCALE, 0.0f, 0.0f, 9.543e-4),		glm::vec4(0.0, 0.0, initialVelocity(5.2),	0.0),	glm::vec4(11.21, 11.21, 11.21,		5), glm::vec4(degToRad(-3.13), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(864), 0.0, 0.0) };		//Jupiter
	objects[6] = { glm::vec4(9.5	* SCALE, 0.0f, 0.0f, 2.857e-4),		glm::vec4(0.0, 0.0, initialVelocity(9.5),	0.0),	glm::vec4(9.45, 9.45, 9.45,			6), glm::vec4(degToRad(-26.73), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(785.448), 0.0, 0.0) };		//Saturn
	objects[7] = { glm::vec4(19.2	* SCALE, 0.0f, 0.0f, 4.365e-5),		glm::vec4(0.0, 0.0, initialVelocity(19.2),	0.0),	glm::vec4(4.01, 4.01, 4.01,			7), glm::vec4(degToRad(-97.77), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(-508.248), 0.0, 0.0) };		//Uranus
	objects[8] = { glm::vec4(30.0	* SCALE, 0.0f, 0.0f, 5.149e-5),		glm::vec4(0.0, 0.0, initialVelocity(30.0),	0.0),	glm::vec4(3.88, 3.88, 3.88,			8), glm::vec4(degToRad(-28.32), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(540), 0.0, 0.0) };		//Neptune
	float mass = 1.201657180090000162e-9 / objectsToSpawn;

	std::vector<glm::vec2> allPoints;
	//float increment = 360 / 200;
	int offset = 0;
	for (int i = 1; i < 9; i++)
	{
		//float theta = -180;
		//std::vector<glm::vec2> points;
		//while (theta != 180)
		//{
		//	points.push_back(glm::vec2(cos(degToRad(theta)) * objects[i].position.x, sin(degToRad(theta)) * objects[i].position.x));
		//	theta += increment;
		//}
		//points.push_back(points[0]);
		std::vector<glm::vec2> points = CalculateOrbitPoints(objects[i].position, objects[i].velocity, G, 0.5 * (objects[i].position.x / SCALE));
		allPoints.insert(allPoints.end(), points.begin(), points.end());
		m_orbitVertexInfo.offsets.push_back(offset);
		offset += points.size();
		m_orbitVertexInfo.vertices.push_back(points.size());
	}

	vko::Buffer vertexStagingBuffer = CreateBuffer(allPoints.size() * sizeof(glm::vec2), vk::BufferUsageFlagBits::eTransferSrc, allPoints.data());
	m_orbitVertexInfo.m_bufferVertexOrbit = CreateBuffer(allPoints.size() * sizeof(glm::vec2), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, nullptr, vk::MemoryPropertyFlagBits::eDeviceLocal);
	CopyBuffer(vertexStagingBuffer, m_orbitVertexInfo.m_bufferVertexOrbit, allPoints.size() * sizeof(glm::vec2));
	vertexStagingBuffer.Destroy();

	for (int i = 9; i < objectsToSpawn; i++)
	{
		glm::vec2 ring0{ 2 * SCALE, 2.7 * SCALE };
		float rho, theta;
		rho = sqrt((pow(ring0[1], 2.0f) - pow(ring0[0], 2.0f)) * uniformDist(rndGenerator) + pow(ring0[0], 2.0f));
		theta = 2.0 * M_PI * uniformDist(rndGenerator);
		objects[i].position = glm::vec4(rho*cos(theta), uniformDist(rndGenerator) * 0.5f - 0.25f, rho*sin(theta), 10e-10);
		float r = sqrt(pow(objects[i].position.x, 2) + pow(objects[i].position.z, 2));
		float vel = initialVelocity(r);

		float mag = sqrt(glm::dot(objects[i].position, objects[i].position));
		glm::vec3 normalisedPos = glm::vec3(objects[i].position.x / mag, objects[i].position.y, objects[i].position.z / mag);
		
		objects[i].velocity = glm::vec4((normalisedPos.z * vel), 0.0f, (-normalisedPos.x * vel), 0.0f); //CW rotation
		objects[i].scale = glm::vec4(0.0, 0.0, 0.0, 0.0);
		objects[i].rotation = glm::vec4(M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), 0.0f);
		objects[i].rotationSpeed = glm::vec4(1.0 * uniformDist(rndGenerator), 1.0 * uniformDist(rndGenerator), 1.0 * uniformDist(rndGenerator), 0.0f);
	}

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
	std::vector<VulkanTools::ImageResources> swapchainImages = m_vulkanResources->swapchain.GetImages();
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

		m_vulkanResources->commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics.orbitPipeline);	
		m_vulkanResources->commandBuffers[i].bindVertexBuffers(0, m_orbitVertexInfo.m_bufferVertexOrbit.buffer, { 0 });
		for (int j = 0; j < m_orbitVertexInfo.vertices.size(); j++)
		{
			m_vulkanResources->commandBuffers[i].draw(m_orbitVertexInfo.vertices[j], 1, m_orbitVertexInfo.offsets[j], 0);
		}

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
		vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1),
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1)
	};

	vk::DescriptorPoolCreateInfo poolInfo = vk::DescriptorPoolCreateInfo({}, 4, poolSizes.size(), poolSizes.data());
	m_vulkanResources->descriptorPool = m_vulkanResources->device.createDescriptorPool(poolInfo);
}

void OrreyVk::CreateDescriptorSetLayout()
{
	std::vector<vk::DescriptorSetLayoutBinding> layoutBindings =
	{
		vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),
		vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
	};

	m_graphics.descriptorSetLayout = m_vulkanResources->device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, layoutBindings.size(), layoutBindings.data()));
}

void OrreyVk::CreateDescriptorSet()
{
	vk::DescriptorSetAllocateInfo allocInfo = vk::DescriptorSetAllocateInfo(m_vulkanResources->descriptorPool, 1, &m_graphics.descriptorSetLayout);
	m_graphics.descriptorSet = m_vulkanResources->device.allocateDescriptorSets(allocInfo)[0];

	std::vector<vk::WriteDescriptorSet> writeSets =
	{
		vk::WriteDescriptorSet(m_graphics.descriptorSet, 2, 0, 1, vk::DescriptorType::eUniformBuffer, {}, &(m_graphics.uniformBuffer.descriptor)),
		vk::WriteDescriptorSet(m_graphics.descriptorSet, 3, 0, 1, vk::DescriptorType::eCombinedImageSampler, &(m_textureArrayPlanets.descriptor), {})
	};

	m_vulkanResources->device.updateDescriptorSets(writeSets.size(), writeSets.data(), 0, nullptr);
}

void OrreyVk::CreateGraphicsPipelineLayout()
{
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo = vk::PipelineLayoutCreateInfo({}, 1, &m_graphics.descriptorSetLayout);
	m_graphics.pipelineLayout = m_vulkanResources->device.createPipelineLayout(pipelineLayoutInfo);
	//pipelineLayoutInfo = vk::PipelineLayoutCreateInfo();
	//m_graphics.pipelineLayoutOrbit = m_vulkanResources->device.createPipelineLayout(pipelineLayoutInfo);
}

void OrreyVk::CreateGraphicsPipeline()
{
	vk::ShaderModule vertShader = CompileShader("resources/shaders/shader.vert", shaderc_shader_kind::shaderc_glsl_vertex_shader);
	vk::ShaderModule fragShader = CompileShader("resources/shaders/shader.frag", shaderc_shader_kind::shaderc_glsl_fragment_shader);

	vk::PipelineShaderStageCreateInfo vertShaderStage = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertShader, "main");
	vk::PipelineShaderStageCreateInfo fragShaderStage = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragShader, "main");
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStage, fragShaderStage };

	std::vector<vk::VertexInputAttributeDescription> vertexAttributeDescriptions = m_sphere.GetVertexAttributeDescription();
	vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription(3, 1, vk::Format::eR32G32B32A32Sfloat, offsetof(CelestialObj, position)));
	vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription(4, 1, vk::Format::eR32G32B32A32Sfloat, offsetof(CelestialObj, scale)));
	vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription(5, 1, vk::Format::eR32G32B32A32Sfloat, offsetof(CelestialObj, rotation)));

	std::vector<vk::VertexInputBindingDescription> bindingDesc = { m_sphere.GetVertexBindingDescription() };
	bindingDesc.push_back(vk::VertexInputBindingDescription(1, sizeof(CelestialObj), vk::VertexInputRate::eInstance));

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo = vk::PipelineVertexInputStateCreateInfo({}, 2, bindingDesc.data(), vertexAttributeDescriptions.size(), vertexAttributeDescriptions.data());

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly = vk::PipelineInputAssemblyStateCreateInfo({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

	vk::Viewport viewport = vk::Viewport(0.0, 0.0, m_vulkanResources->swapchain.GetDimensions().width, m_vulkanResources->swapchain.GetDimensions().height, 0.0, 1.0);
	vk::Rect2D scissor = vk::Rect2D({ 0,0 }, m_vulkanResources->swapchain.GetDimensions());
	vk::PipelineViewportStateCreateInfo viewPortState = vk::PipelineViewportStateCreateInfo({}, 1, &viewport, 1, &scissor);

	vk::PipelineRasterizationStateCreateInfo rastierizer = vk::PipelineRasterizationStateCreateInfo();
	rastierizer.cullMode = vk::CullModeFlagBits::eBack;
	rastierizer.frontFace = vk::FrontFace::eClockwise;

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

	inputAssembly = vk::PipelineInputAssemblyStateCreateInfo({}, vk::PrimitiveTopology::eLineStrip, VK_FALSE);

	vertexAttributeDescriptions = { vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, 0) };
	bindingDesc = { vk::VertexInputBindingDescription(0, sizeof(glm::vec2)) };

	vertShader = CompileShader("resources/shaders/orbit.vert", shaderc_shader_kind::shaderc_glsl_vertex_shader);
	fragShader = CompileShader("resources/shaders/orbit.frag", shaderc_shader_kind::shaderc_glsl_fragment_shader);

	shaderStages[0] = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertShader, "main");
	shaderStages[1] = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragShader, "main");

	vertexInputInfo = vk::PipelineVertexInputStateCreateInfo({}, 1, bindingDesc.data(), vertexAttributeDescriptions.size(), vertexAttributeDescriptions.data());

	//pipelineInfo.layout = m_graphics.pipelineLayoutOrbit;
	m_graphics.orbitPipeline = m_vulkanResources->device.createGraphicsPipeline(nullptr, pipelineInfo);

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
	if (m_speed < 0)
		m_speed = 0;
	m_compute.ubo.speed = m_speed;
	memcpy(m_compute.uniformBuffer.mapped, &m_compute.ubo, sizeof(m_compute.ubo));
}

void OrreyVk::PrepareCompute()
{
	//Compute Uniform buffer
	m_compute.uniformBuffer = CreateBuffer(sizeof(m_compute.ubo), vk::BufferUsageFlagBits::eUniformBuffer);
	m_compute.ubo.objectCount = m_bufferInstance.size / sizeof(CelestialObj);
	m_compute.ubo.scale = SCALE;
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

std::vector<glm::vec2> OrreyVk::CalculateOrbitPoints(glm::vec4 pos, glm::vec4 vel, double G, float timestep)
{
	int plotPoints = ceil(400 / timestep) * pos.x; //This seems to work quite well
	float xT = pos.x / SCALE;
	float zT = pos.z / SCALE;
	float xV = vel.x;
	float zV = vel.z;
	std::vector<glm::vec2> plotPositions;

	for (int i = 0; i < plotPoints; i++)
	{
		float radius = sqrt((xT * xT) + (zT * zT));
		float gravAcc = (G * pos.w) / (radius * radius);
		float angle = atan2(xT, zT);

		float acc = gravAcc / pos.w;

		xV += (sin(angle) * acc * timestep);
		zV += (cos(angle) * acc * timestep);

		xT -= xV * timestep;
		zT -= zV * timestep;
		
		plotPositions.emplace_back(glm::vec2(xT * SCALE,zT * SCALE));
	}

	return plotPositions;
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

		m_totalRunTime += m_frameTime;
		if (m_totalRunTime >= m_seconds)
		{
			//spdlog::info("\tRuntime = {}", m_seconds);
			m_seconds += 1;
		}
	}
}

void OrreyVk::Cleanup() {
	m_vulkanResources->device.waitIdle();
	m_bufferVertex.Destroy();
	m_bufferIndex.Destroy();
	m_bufferInstance.Destroy();
	m_orbitVertexInfo.m_bufferVertexOrbit.Destroy();
	m_textureArrayPlanets.Destroy();

	m_graphics.uniformBuffer.Destroy();
	m_vulkanResources->device.destroyDescriptorSetLayout(m_graphics.descriptorSetLayout);
	m_vulkanResources->device.destroyPipeline(m_graphics.pipeline);
	m_vulkanResources->device.destroyPipelineLayout(m_graphics.pipelineLayout);
	m_vulkanResources->device.destroySemaphore(m_graphics.semaphore);

	m_compute.uniformBuffer.Destroy();
	m_compute.commandPool.Destroy();
	m_vulkanResources->device.destroyDescriptorSetLayout(m_compute.descriptorSetLayout);
	m_vulkanResources->device.destroyPipeline(m_compute.pipeline);
	m_vulkanResources->device.destroyPipelineLayout(m_compute.pipelineLayout);
	m_vulkanResources->device.destroySemaphore(m_compute.semaphore);
	m_vulkanResources->device.destroyFence(m_compute.fence);
	Vulkan::Cleanup();

	glfwDestroyWindow(m_window);

	glfwTerminate();
}