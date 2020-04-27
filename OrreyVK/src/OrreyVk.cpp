#include "OrreyVk.h"

#define OBJECTS_PER_GROUP 512
#define SATURN_RING_OBJECT_COUNT 6000
#define ASTROID_BELT_MAX_OBJECT_COUNT 250000
#define SCALE 30

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

	m_window = glfwCreateWindow(1980, 1080, "OrreyVk", nullptr, nullptr);

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

	m_graphics.uniformBuffer = CreateBuffer(sizeof(m_graphics.ubo), vk::BufferUsageFlagBits::eUniformBuffer);
	m_graphics.uniformBuffer.Map();
	memcpy(m_graphics.uniformBuffer.mapped, &m_graphics.ubo, sizeof(m_graphics.ubo));

	vertexStagingBuffer.Destroy();
	indexStagingBuffer.Destroy();

	m_graphics.semaphore = m_vulkanResources->device.createSemaphore(vk::SemaphoreCreateInfo());

	//Load textures in, from: https://www.solarsystemscope.com/textures/
	std::vector<const char*> paths = { 
		"resources/sun.jpg", 
		"resources/mercury.jpg",
		"resources/venus.jpg",
		"resources/earth.jpg",
		"resources/mars.jpg",
		"resources/jupiter.jpg",
		"resources/saturn.jpg",
		"resources/uranus.jpg",
		"resources/neptune.jpg",
		"resources/moon.jpg"
	};

	m_textureArrayPlanets = Create2DTextureArray(vk::Format::eR8G8B8A8Unorm, paths, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, true);
	m_textureStarfield = CreateTexture(vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, "resources/starsmilkyway8k.jpg", vk::ImageUsageFlagBits::eSampled, true);
	
	
	//Create query pool to time compute, and rendering times
	vk::QueryPoolCreateInfo queryPoolInfo = vk::QueryPoolCreateInfo({}, vk::QueryType::eTimestamp, 2, {});
	m_queryPool = m_vulkanResources->device.createQueryPool(queryPoolInfo);
	m_queryResults.resize(2);

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

	int objectsToSpawn = 13 + SATURN_RING_OBJECT_COUNT + ASTROID_BELT_MAX_OBJECT_COUNT;
	while (objectsToSpawn % OBJECTS_PER_GROUP != 0) //Reduce astroid count until it divides nicely with objects per group
		objectsToSpawn--;

	//Converted G constant for AU/SM, T: 1s ~= 1 earth sidereal day
	const double G = 0.0002959122083;
	auto initialVelocity = [G](float r, float mass = 1.0) { return sqrt((G * mass) / r);  };
	auto degToRad = [](float deg) {return deg * M_PI / 180; };

	objects.resize(objectsToSpawn);
	objects[0] = { glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), glm::vec4(0.0, 0.0, 0.0, 0.0), glm::vec4(5, 5, 5, 0), glm::vec4(degToRad(-7.25), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(15.228), 0.0, 0.0) }; //Sun
	//				Distance in AU						 Mass in Solar Mass,				Velocity in AU					Scale as ratio of Earth			Texture index   Rotation in degrees					Rotation Speed 
	objects[1] = { glm::vec4(0.39	* SCALE, 0.0f, 0.0f, 1.651502e-7),	glm::vec4(0.0, 0.0,	initialVelocity(0.39),	0.0),	glm::vec4(0.37, 0.37, 0.37,			1),	glm::vec4(degToRad(-0.01), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(6.12), 0.0, 0.0) }; //Mercury
	objects[2] = { glm::vec4(0.723	* SCALE, 0.0f, 0.0f, 2.447225e-6),	glm::vec4(0.0, 0.0, initialVelocity(0.723), 0.0),	glm::vec4(0.949, 0.949, 0.949,		2), glm::vec4(degToRad(-117.4), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(-1.476), 0.0, 0.0) }; //Venus
	objects[3] = { glm::vec4(1.0	* SCALE, 0.0f, 0.0f, 3.0027e-6),	glm::vec4(0.0, 0.0, initialVelocity(1.0),	0.0),	glm::vec4(1.0, 1.0, 1.0,			3), glm::vec4(degToRad(-23.5), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(360), 0.0, 0.0) }; //Earth
	objects[4] = { glm::vec4(1.524	* SCALE, 0.0f, 0.0f, 3.212921e-7),	glm::vec4(0.0, 0.0, initialVelocity(1.524), 0.0),	glm::vec4(0.532, 0.532, 0.532,		4), glm::vec4(degToRad(-25.19), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(345.6), 0.0, 0.0) }; //Mars
	objects[5] = { glm::vec4(5.2	* SCALE, 0.0f, 0.0f, 9.543e-4),		glm::vec4(0.0, 0.0, initialVelocity(5.2),	0.0),	glm::vec4(10.97, 10.97, 10.97,		5), glm::vec4(degToRad(-3.13), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(864), 0.0, 0.0) }; //Jupiter
	objects[6] = { glm::vec4(9.5	* SCALE, 0.0f, 0.0f, 2.857e-4),		glm::vec4(0.0, 0.0, initialVelocity(9.5),	0.0),	glm::vec4(9.14, 9.14, 9.14,			6), glm::vec4(degToRad(-26.73), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(785.448), 0.0, 0.0) }; //Saturn
	objects[7] = { glm::vec4(19.2	* SCALE, 0.0f, 0.0f, 4.365e-5),		glm::vec4(0.0, 0.0, initialVelocity(19.2),	0.0),	glm::vec4(3.98, 3.98, 3.98,			7), glm::vec4(degToRad(-97.77), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(-508.248), 0.0, 0.0) }; //Uranus
	objects[8] = { glm::vec4(30.0	* SCALE, 0.0f, 0.0f, 5.149e-5),		glm::vec4(0.0, 0.0, initialVelocity(30.0),	0.0),	glm::vec4(3.86, 3.86, 3.86,			8), glm::vec4(degToRad(-28.32), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(540), 0.0, 0.0) }; //Neptune

	//Moons
	objects[9] = { glm::vec4((0.15) * SCALE, 0.0f, 0.0f, 3.69432e-8), glm::vec4(0.0, 0.0, initialVelocity(0.15),		0.0),	glm::vec4(0.27, 0.27, 0.27,			9), glm::vec4(degToRad(-1.5), 0.0, 0.0, 0.0),	glm::vec4(0.0f, degToRad(13.33), 0.0, 0.0), glm::vec4(objects[3].position.x, objects[3].position.y, objects[3].position.z, 3.0) };		//Moon
	objects[9].orbitalTilt = glm::vec4(degToRad(5.14), 0.0, 0.0, 0.0);
	
	objects[10] = { glm::vec4((2.2) * SCALE, 0.0f, 0.0f,	3.69432e-8), glm::vec4(0.0, 0.0, initialVelocity(2.2),		0.0),	glm::vec4(0.4, 0.4, 0.4,			6),glm::vec4(degToRad(0.0), 0.0, 0.0, 0.0),		glm::vec4(0.0f, degToRad(22.5), 0.0, 0.0), glm::vec4(objects[6].position.x, objects[6].position.y, objects[6].position.z, 6.0) };		//Titan
	objects[10].colourTint = glm::vec4(0.9, 0.89, 0.36, 1.0);
	objects[10].orbitalTilt = glm::vec4(degToRad(-26.73), 0.0, 0.0, 0.0);

	objects[11] = { glm::vec4((1.5) * SCALE, 0.0f, 0.0f,	3.69432e-8), glm::vec4(0.0, 0.0, initialVelocity(1.5),		0.0),	glm::vec4(0.12, 0.12, 0.12,			1),glm::vec4(degToRad(0.0), 0.0, 0.0, 0.0),		glm::vec4(0.0f, degToRad(80.0), 0.0, 0.0), glm::vec4(objects[6].position.x, objects[6].position.y, objects[6].position.z, 6.0) };		//Rhea
	objects[11].orbitalTilt = glm::vec4(degToRad(-26.73), 0.0, 0.0, 0.0);

	objects[12] = { glm::vec4((1.8) * SCALE, 0.0f, 0.0f,	3.69432e-8), glm::vec4(0.0, 0.0, initialVelocity(1.8),		0.0),	glm::vec4(0.41, 0.41, 0.41,			1),glm::vec4(degToRad(0.0), 0.0, 0.0, 0.0),		glm::vec4(0.0f, degToRad(50.52), 0.0, 0.0), glm::vec4(objects[5].position.x, objects[5].position.y, objects[5].position.z, 5.0) };		//Ganymede
	objects[12].orbitalTilt = glm::vec4(degToRad(-3.13), 0.0, 0.0, 0.0);

	//Saturn ring
	for (int i = 13; i < SATURN_RING_OBJECT_COUNT + 13; i++)
	{
		glm::vec2 ring0{ 0.3 * SCALE, 0.8 * SCALE };
		float rho, theta;
		rho = sqrt((pow(ring0[1], 2.0f) - pow(ring0[0], 2.0f)) * uniformDist(rndGenerator) + pow(ring0[0], 2.0f));
		theta = 2.0 * M_PI * uniformDist(rndGenerator);
		objects[i].position = glm::vec4(rho*cos(theta), uniformDist(rndGenerator) * 0.5f - 0.25f, rho*sin(theta), 3.69432e-32);
		float r = sqrt(pow(objects[i].position.x, 2) + pow(objects[i].position.z, 2));
		float vel = initialVelocity(r /SCALE); //Velocity calculation needs to be in unscaled AU
	
		float mag = sqrt(glm::dot(objects[i].position, objects[i].position));
		glm::vec3 normalisedPos = glm::vec3(objects[i].position.x / mag, objects[i].position.y, objects[i].position.z / mag);
	
		objects[i].velocity = glm::vec4((-normalisedPos.z * vel), 0.0f, (normalisedPos.x * vel), 0.0f); //CCW rotation
		objects[i].scale = glm::vec4(RandomRange(0.01, 0.07), RandomRange(0.01, 0.07), RandomRange(0.01, 0.07), 1);
		objects[i].rotation = glm::vec4(M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), 0.0f);
		objects[i].rotationSpeed = glm::vec4(1.0 * uniformDist(rndGenerator), 1.0 * uniformDist(rndGenerator), 1.0 * uniformDist(rndGenerator), 0.0f);
		objects[i].posOffset = glm::vec4(objects[6].position.x, objects[6].position.y, objects[6].position.z, 6.0);
		objects[i].orbitalTilt = objects[6].rotation;
		float colourTint = 0.5 * uniformDist(rndGenerator) + 0.4;
		objects[i].colourTint = glm::vec4(colourTint, colourTint, colourTint, 1.0);
	}
	
	//Astroid belt
	for (int i = SATURN_RING_OBJECT_COUNT + 13; i < objectsToSpawn; i++)
	{
		glm::vec2 ring0{ 2 * SCALE, 3 * SCALE };
		float rho, theta;
		rho = sqrt((pow(ring0[1], 2.0f) - pow(ring0[0], 2.0f)) * uniformDist(rndGenerator) + pow(ring0[0], 2.0f));
		theta = 2.0 * M_PI * uniformDist(rndGenerator);
		objects[i].position = glm::vec4(rho*cos(theta), uniformDist(rndGenerator) * 0.5f - 0.25f, rho*sin(theta), 3.69432e-32);
		float r = sqrt(pow(objects[i].position.x, 2) + pow(objects[i].position.z, 2));
		float vel = initialVelocity(r / SCALE);
	
		float mag = sqrt(glm::dot(objects[i].position, objects[i].position));
		glm::vec3 normalisedPos = glm::vec3(objects[i].position.x / mag, objects[i].position.y, objects[i].position.z / mag);
		
		objects[i].velocity = glm::vec4((normalisedPos.z * vel), 0.0f, (-normalisedPos.x * vel), 0.0f); //CW rotation
		objects[i].scale = glm::vec4(RandomRange(0.01, 0.07), RandomRange(0.01, 0.07), RandomRange(0.01, 0.07), 1);
		objects[i].rotation = glm::vec4(M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), 0.0f);
		objects[i].rotationSpeed = glm::vec4(1.0 * uniformDist(rndGenerator), 1.0 * uniformDist(rndGenerator), 1.0 * uniformDist(rndGenerator), 0.0f);
		float colourTint = 0.5 * uniformDist(rndGenerator) + 0.4;
		objects[i].colourTint = glm::vec4(colourTint, colourTint, colourTint, 1.0);
	}

	//Upload instance data into its buffer
	uint32_t size = objects.size() * sizeof(CelestialObj);
	vko::Buffer instanceStagingBuffer = CreateBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, objects.data());
	m_bufferInstance = CreateBuffer(size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, nullptr, vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::CommandBuffer cmdBuffer = m_vulkanResources->commandPool.AllocateCommandBuffer();
	cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	//CopyBuffer(instanceStagingBuffer, m_bufferInstance, size);
	cmdBuffer.copyBuffer(instanceStagingBuffer.buffer, m_bufferInstance.buffer, vk::BufferCopy(0, 0, size));

	//Give compute queue ownership of the instance buffer so it's ready for the render loop
	if (m_queueIDs.graphics.familyID != m_queueIDs.compute.familyID)
	{
		InsertBufferMemoryBarrier(cmdBuffer, m_bufferInstance,
			vk::AccessFlagBits::eVertexAttributeRead, {},
			vk::PipelineStageFlagBits::eVertexInput, vk::PipelineStageFlagBits::eComputeShader,
			m_queueIDs.graphics.familyID, m_queueIDs.compute.familyID);
	}

	cmdBuffer.end();
	vk::SubmitInfo submitInfo = vk::SubmitInfo();
	vk::Fence fence = m_vulkanResources->device.createFence(vk::FenceCreateInfo());
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;
	m_vulkanResources->queueGraphics.submit(submitInfo, fence);
	m_vulkanResources->device.waitForFences(fence, true, UINT64_MAX);
	m_vulkanResources->device.destroyFence(fence);
	m_vulkanResources->commandPool.FreeCommandBuffers(cmdBuffer);

	instanceStagingBuffer.Destroy();

	//Roughly calculate the orbits for drawing
	std::vector<glm::vec2> allPoints;
	int offset = 0;
	for (int i = 1; i < 9; i++)
	{
		std::vector<glm::vec2> points = CalculateOrbitPoints(objects[i].position, objects[i].velocity, G, 0.5 * (objects[i].position.x / SCALE));
		allPoints.insert(allPoints.end(), points.begin(), points.end());
		m_orbitVertexInfo.offsets.push_back(offset);
		offset += points.size();
		m_orbitVertexInfo.vertices.push_back(points.size());
	}

	//Copy orbit data into its vertex buffer (drawn as lines)
	vko::Buffer vertexStagingBuffer = CreateBuffer(allPoints.size() * sizeof(glm::vec2), vk::BufferUsageFlagBits::eTransferSrc, allPoints.data());
	m_orbitVertexInfo.m_bufferVertexOrbit = CreateBuffer(allPoints.size() * sizeof(glm::vec2), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, nullptr, vk::MemoryPropertyFlagBits::eDeviceLocal);
	
	CopyBuffer(vertexStagingBuffer, m_orbitVertexInfo.m_bufferVertexOrbit, allPoints.size() * sizeof(glm::vec2));
	vertexStagingBuffer.Destroy();
}

void OrreyVk::CreateCommandBuffers()
{
	uint32_t swapchainLength = m_vulkanResources->swapchain.GetImageCount();
	m_vulkanResources->commandBuffers = m_vulkanResources->commandPool.AllocateCommandBuffers(swapchainLength);
	std::vector<VulkanTools::ImageResources> swapchainImages = m_vulkanResources->swapchain.GetImages();
	vk::Extent2D swapchainDimensions = m_vulkanResources->swapchain.GetDimensions();
	for (size_t i = 0; i < swapchainLength; i++)
	{
		std::vector<vk::ClearValue> clearValues = {};
		clearValues.resize(2);
		clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
		clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0.0f);
		vk::RenderPassBeginInfo renderPassInfo = vk::RenderPassBeginInfo(m_vulkanResources->renderpass, m_vulkanResources->frameBuffers[i]);
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues.data();
		renderPassInfo.renderArea = vk::Rect2D({ 0, 0 }, swapchainDimensions);

		m_vulkanResources->commandBuffers[i].begin(vk::CommandBufferBeginInfo());

		if (m_queueIDs.graphics.familyID != m_queueIDs.compute.familyID)
		{
			InsertBufferMemoryBarrier(m_vulkanResources->commandBuffers[i], m_bufferInstance,
				{}, vk::AccessFlagBits::eVertexAttributeRead,
				vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexInput,
				m_queueIDs.compute.familyID, m_queueIDs.graphics.familyID);
		}

		m_vulkanResources->commandBuffers[i].resetQueryPool(m_queryPool, 0, 2);
		m_vulkanResources->commandBuffers[i].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
		
		//Draw sky sphere
		m_vulkanResources->commandBuffers[i].bindVertexBuffers(0, m_bufferVertex.buffer, { 0 });
		m_vulkanResources->commandBuffers[i].bindIndexBuffer(m_bufferIndex.buffer, { 0 }, vk::IndexType::eUint16);
		m_vulkanResources->commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics.pipelineSkySphere.pipeline);
		m_vulkanResources->commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphics.pipelinePlanets.layout, 0, 1, &m_graphics.skySphereDescriptorSet, 0, nullptr);
		m_vulkanResources->commandBuffers[i].drawIndexed(m_sphere.GetIndicies().size(), 1, 0, 0, 0);

		//Draw instanced objects
		m_vulkanResources->commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics.pipelinePlanets.pipeline);
		m_vulkanResources->commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphics.pipelinePlanets.layout, 0, 1, &m_graphics.descriptorSet, 0, nullptr);
		m_vulkanResources->commandBuffers[i].bindVertexBuffers(1, m_bufferInstance.buffer, { 0 });
		m_vulkanResources->commandBuffers[i].writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_queryPool, 0);
		m_vulkanResources->commandBuffers[i].drawIndexed(m_sphere.GetIndicies().size(), m_bufferInstance.size / sizeof(CelestialObj), 0, 0, 0);
		m_vulkanResources->commandBuffers[i].writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_queryPool, 1);

		//Draw orbits
		m_vulkanResources->commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics.pipelineOrbits.pipeline);	
		m_vulkanResources->commandBuffers[i].bindVertexBuffers(0, m_orbitVertexInfo.m_bufferVertexOrbit.buffer, { 0 });
		for (int j = 0; j < m_orbitVertexInfo.vertices.size(); j++)
		{
			m_vulkanResources->commandBuffers[i].draw(m_orbitVertexInfo.vertices[j], 1, m_orbitVertexInfo.offsets[j], 0);
		}

		m_vulkanResources->commandBuffers[i].endRenderPass();

		if (m_queueIDs.graphics.familyID != m_queueIDs.compute.familyID)
		{
			InsertBufferMemoryBarrier(m_vulkanResources->commandBuffers[i], m_bufferInstance,
				vk::AccessFlagBits::eVertexAttributeRead, {},
				vk::PipelineStageFlagBits::eVertexInput, vk::PipelineStageFlagBits::eComputeShader,
				m_queueIDs.graphics.familyID, m_queueIDs.compute.familyID);
		}

		m_vulkanResources->commandBuffers[i].end();
	}
}

void OrreyVk::CreateDescriptorPool()
{
	std::vector<vk::DescriptorPoolSize> poolSizes =
	{
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 3),
		vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1),
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 2)
	};

	vk::DescriptorPoolCreateInfo poolInfo = vk::DescriptorPoolCreateInfo({}, 6, poolSizes.size(), poolSizes.data());
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
	vk::DescriptorSetLayout layouts[] = { m_graphics.descriptorSetLayout, m_graphics.descriptorSetLayout };
	vk::DescriptorSetAllocateInfo allocInfo = vk::DescriptorSetAllocateInfo(m_vulkanResources->descriptorPool, 2, layouts);
	std::vector<vk::DescriptorSet> sets = m_vulkanResources->device.allocateDescriptorSets(allocInfo);
	m_graphics.descriptorSet = sets[0];
	m_graphics.skySphereDescriptorSet = sets[1];

	std::vector<vk::WriteDescriptorSet> writeSets =
	{
		vk::WriteDescriptorSet(m_graphics.descriptorSet, 2, 0, 1, vk::DescriptorType::eUniformBuffer, {}, &(m_graphics.uniformBuffer.descriptor)),
		vk::WriteDescriptorSet(m_graphics.descriptorSet, 3, 0, 1, vk::DescriptorType::eCombinedImageSampler, &(m_textureArrayPlanets.descriptor), {}),

		vk::WriteDescriptorSet(m_graphics.skySphereDescriptorSet, 2, 0, 1, vk::DescriptorType::eUniformBuffer, {}, &(m_graphics.uniformBuffer.descriptor)),
		vk::WriteDescriptorSet(m_graphics.skySphereDescriptorSet, 3, 0, 1, vk::DescriptorType::eCombinedImageSampler, &(m_textureStarfield.descriptor), {})
	};

	m_vulkanResources->device.updateDescriptorSets(writeSets.size(), writeSets.data(), 0, nullptr);
}

void OrreyVk::CreateGraphicsPipelineLayout()
{
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo = vk::PipelineLayoutCreateInfo({}, 1, &m_graphics.descriptorSetLayout);
	m_graphics.pipelinePlanets.layout = m_vulkanResources->device.createPipelineLayout(pipelineLayoutInfo);
}

void OrreyVk::CreateGraphicsPipeline()
{
	vk::ShaderModule vertShader = CompileShader("resources/shaders/planets.vert.spv");
	vk::ShaderModule fragShader = CompileShader("resources/shaders/planets.frag.spv");

	vk::PipelineShaderStageCreateInfo vertShaderStage = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertShader, "main");
	vk::PipelineShaderStageCreateInfo fragShaderStage = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragShader, "main");
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStage, fragShaderStage };

	std::vector<vk::VertexInputAttributeDescription> vertexAttributeDescriptions = m_sphere.GetVertexAttributeDescription();
	vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription(3, 1, vk::Format::eR32G32B32A32Sfloat, offsetof(CelestialObj, position)));
	vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription(4, 1, vk::Format::eR32G32B32A32Sfloat, offsetof(CelestialObj, posOffset)));
	vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription(5, 1, vk::Format::eR32G32B32A32Sfloat, offsetof(CelestialObj, scale)));
	vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription(6, 1, vk::Format::eR32G32B32A32Sfloat, offsetof(CelestialObj, rotation)));
	vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription(7, 1, vk::Format::eR32G32B32A32Sfloat, offsetof(CelestialObj, orbitalTilt)));
	vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription(8, 1, vk::Format::eR32G32B32A32Sfloat, offsetof(CelestialObj, colourTint)));

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
	msState.rasterizationSamples = m_msaaSamples;

	vk::PipelineDepthStencilStateCreateInfo depthStencilInfo = vk::PipelineDepthStencilStateCreateInfo({}, VK_TRUE, VK_TRUE, vk::CompareOp::eLess);

	vk::PipelineColorBlendAttachmentState colourBlendAttachState = vk::PipelineColorBlendAttachmentState();
	colourBlendAttachState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	vk::PipelineColorBlendStateCreateInfo colourBlendInfo = vk::PipelineColorBlendStateCreateInfo();
	colourBlendInfo.pAttachments = &colourBlendAttachState;
	colourBlendInfo.attachmentCount = 1;

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

	pipelineInfo.layout = m_graphics.pipelinePlanets.layout;
	pipelineInfo.renderPass = m_vulkanResources->renderpass;
	pipelineInfo.subpass = 0;

	m_graphics.pipelinePlanets.pipeline = m_vulkanResources->device.createGraphicsPipeline(nullptr, pipelineInfo);

	//Orbit pipeline - Uses same layout as planets(ubo, texture array)- we just don't access the array in the fragment shader
	inputAssembly.topology = vk::PrimitiveTopology::eLineStrip;
	vertexAttributeDescriptions = { vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, 0) }; //Change vertex input
	bindingDesc = { vk::VertexInputBindingDescription(0, sizeof(glm::vec2)) };
	vertexInputInfo = vk::PipelineVertexInputStateCreateInfo({}, 1, bindingDesc.data(), vertexAttributeDescriptions.size(), vertexAttributeDescriptions.data());
	vertShader = CompileShader("resources/shaders/orbit.vert.spv"); //Change shaders
	fragShader = CompileShader("resources/shaders/orbit.frag.spv");
	shaderStages[0] = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertShader, "main");
	shaderStages[1] = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragShader, "main");
	m_graphics.pipelineOrbits.pipeline = m_vulkanResources->device.createGraphicsPipeline(nullptr, pipelineInfo);

	//Skysphere Pipeline
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	vertexAttributeDescriptions = m_sphere.GetVertexAttributeDescription();
	vertShader = CompileShader("resources/shaders/skysphere.vert.spv");
	fragShader = CompileShader("resources/shaders/skysphere.frag.spv");
	shaderStages[0] = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertShader, "main");
	shaderStages[1] = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragShader, "main");
	rastierizer.frontFace = vk::FrontFace::eCounterClockwise; //So we can see the texture from inside the sphere
	bindingDesc = { m_sphere.GetVertexBindingDescription() };
	vertexInputInfo = vk::PipelineVertexInputStateCreateInfo({}, 1, bindingDesc.data(), vertexAttributeDescriptions.size(), vertexAttributeDescriptions.data());

	m_graphics.pipelineSkySphere.pipeline = m_vulkanResources->device.createGraphicsPipeline(nullptr, pipelineInfo);
	
	m_vulkanResources->device.destroyShaderModule(vertShader);
	m_vulkanResources->device.destroyShaderModule(fragShader);
}

void OrreyVk::RenderFrame()
{
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

	m_vulkanResources->queueGraphics.submit(submitInfo, nullptr);

	vk::SwapchainKHR swapchains[] = { m_vulkanResources->swapchain.GetVkObject() };
	vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR(1, &m_vulkanResources->semaphoreRender[m_frameID], 1, swapchains, &imageIndex.value);

	vk::Result result = m_vulkanResources->queueGraphics.presentKHR(presentInfo);
	m_vulkanResources->queueGraphics.waitIdle();

	//spdlog::info("\Instance draw time = {}ms", GetTimeQueryResult(m_queueIDs.graphics.timestampValidBits));

	vk::SubmitInfo computeSubmitInfo = vk::SubmitInfo();
	computeSubmitInfo.waitSemaphoreCount = 1;
	computeSubmitInfo.pWaitSemaphores = &m_graphics.semaphore;
	vk::PipelineStageFlags computeWaitStages[] = { vk::PipelineStageFlagBits::eComputeShader };
	computeSubmitInfo.pWaitDstStageMask = computeWaitStages;
	computeSubmitInfo.signalSemaphoreCount = 1;
	computeSubmitInfo.pSignalSemaphores = &m_compute.semaphore;
	computeSubmitInfo.commandBufferCount = 1;
	computeSubmitInfo.pCommandBuffers = &m_compute.cmdBuffer;

	m_vulkanResources->queueCompute.submit(computeSubmitInfo, nullptr);

	//spdlog::info("\Compute time = {}ms", GetTimeQueryResult(m_queueIDs.compute.timestampValidBits));

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
	if (m_speed > 300)
		m_speed = 300;
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
	vk::ShaderModule computeShader = CompileShader("resources/shaders/planets.comp.spv");
	vk::PipelineShaderStageCreateInfo computeShaderStage = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eCompute, computeShader, "main");

	uint32_t objectsPerGroup = OBJECTS_PER_GROUP;
	vk::SpecializationMapEntry specEntry = vk::SpecializationMapEntry(0, 0, sizeof(uint32_t));
	vk::SpecializationInfo specInfo = vk::SpecializationInfo(1, &specEntry, sizeof(uint32_t), &objectsPerGroup);
	computeShaderStage.pSpecializationInfo = &specInfo;

	pipelineCreateInfo.stage = computeShaderStage;
	m_compute.pipeline = m_vulkanResources->device.createComputePipeline(nullptr, pipelineCreateInfo);

	m_compute.commandPool = vko::VulkanCommandPool(m_vulkanResources->device, m_queueIDs.compute.familyID, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	m_compute.cmdBuffer = m_compute.commandPool.AllocateCommandBuffer();

	m_compute.semaphore = m_vulkanResources->device.createSemaphore(vk::SemaphoreCreateInfo());
	vk::SubmitInfo submitInfo = vk::SubmitInfo();
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_compute.semaphore;
	m_vulkanResources->queueCompute.submit(submitInfo, nullptr);
	m_vulkanResources->queueCompute.waitIdle();

	CreateComputeCommandBuffer();

	if (m_queueIDs.graphics.familyID != m_queueIDs.compute.familyID)
	{
		vk::CommandBuffer cmdBuffer = m_compute.commandPool.AllocateCommandBuffer();
		cmdBuffer.begin(vk::CommandBufferBeginInfo());

		InsertBufferMemoryBarrier(cmdBuffer, m_bufferInstance,
			{}, vk::AccessFlagBits::eShaderWrite,
			vk::PipelineStageFlagBits::eVertexInput, vk::PipelineStageFlagBits::eComputeShader,
			m_queueIDs.graphics.familyID, m_queueIDs.compute.familyID);

		InsertBufferMemoryBarrier(cmdBuffer, m_bufferInstance,
			vk::AccessFlagBits::eShaderWrite, {},
			vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexInput,
			m_queueIDs.compute.familyID, m_queueIDs.graphics.familyID);

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
}

void OrreyVk::CreateComputeCommandBuffer()
{
	m_compute.cmdBuffer.begin(vk::CommandBufferBeginInfo());

	InsertBufferMemoryBarrier(m_compute.cmdBuffer, m_bufferInstance,
		{}, vk::AccessFlagBits::eShaderWrite,
		vk::PipelineStageFlagBits::eVertexInput, vk::PipelineStageFlagBits::eComputeShader,
		m_queueIDs.graphics.familyID, m_queueIDs.compute.familyID);
	
	m_compute.cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute.pipeline);
	m_compute.cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_compute.pipelineLayout, 0, m_compute.descriptorSet, {});
	m_compute.cmdBuffer.resetQueryPool(m_queryPool, 0, 2);
	m_compute.cmdBuffer.writeTimestamp(vk::PipelineStageFlagBits::eComputeShader, m_queryPool, 0);
	m_compute.cmdBuffer.dispatch(ceil((m_bufferInstance.size / sizeof(CelestialObj)) / OBJECTS_PER_GROUP), 1, 1);
	m_compute.cmdBuffer.writeTimestamp(vk::PipelineStageFlagBits::eComputeShader, m_queryPool, 1);

	InsertBufferMemoryBarrier(m_compute.cmdBuffer, m_bufferInstance,
		vk::AccessFlagBits::eShaderWrite, {},
		vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexInput,
		m_queueIDs.compute.familyID, m_queueIDs.graphics.familyID);

	m_compute.cmdBuffer.end();
}

std::vector<glm::vec2> OrreyVk::CalculateOrbitPoints(glm::vec4 pos, glm::vec4 vel, double G, float timestep)
{
	int plotPoints = ceil(400 / timestep) * pos.x; //This seems to work quite well
	glm::vec3 vPos = glm::vec3(pos.x, pos.y, pos.z);
	float xT = vPos.x / SCALE;
	float yT = vPos.y / SCALE;
	float zT = vPos.z / SCALE;

	float xV = vel.x;
	float zV = vel.z;
	std::vector<glm::vec2> plotPositions;

	for (int i = 0; i < plotPoints; i++)
	{
		float radiusSquared = (xT * xT) + (zT * zT);
		glm::vec3 forceDir = glm::normalize(glm::vec3(xT, yT, zT));
		glm::vec3 gravAcc = glm::vec3(forceDir.x * G, forceDir.y * G, forceDir.z * G) / radiusSquared;

		xV += (gravAcc.x * timestep);
		zV += (gravAcc.z * timestep);

		xT -= xV * timestep;
		zT -= zV * timestep;
		
		plotPositions.emplace_back(glm::vec2(xT * SCALE,zT * SCALE));
	}

	return plotPositions;
}

double OrreyVk::GetTimeQueryResult(uint32_t timeStampValidBits)
{
	std::array<std::uint64_t, 2> timeStamps = { {0} };
	m_vulkanResources->device.getQueryPoolResults<std::uint64_t>(m_queryPool, 0, 2, timeStamps, sizeof(std::uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);

	std::transform(
		timeStamps.begin(), timeStamps.end(), timeStamps.begin(),
		std::bind(
			glm::bitfieldExtract<std::uint64_t>,
			std::placeholders::_1,
			0,
			timeStampValidBits
		)
	);

	return static_cast<double>((timeStamps[1] - timeStamps[0])) / 1000000.0;
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
	m_textureStarfield.Destroy();

	m_graphics.uniformBuffer.Destroy();
	m_vulkanResources->device.destroyDescriptorSetLayout(m_graphics.descriptorSetLayout);

	m_vulkanResources->device.destroyPipeline(m_graphics.pipelinePlanets.pipeline);
	m_vulkanResources->device.destroyPipeline(m_graphics.pipelineOrbits.pipeline);
	m_vulkanResources->device.destroyPipeline(m_graphics.pipelineSkySphere.pipeline);
	m_vulkanResources->device.destroyPipelineLayout(m_graphics.pipelinePlanets.layout);
	m_vulkanResources->device.destroyPipelineLayout(m_graphics.pipelineOrbits.layout);
	m_vulkanResources->device.destroyPipelineLayout(m_graphics.pipelineSkySphere.layout);

	m_vulkanResources->device.destroySemaphore(m_graphics.semaphore);

	m_compute.uniformBuffer.Destroy();
	m_compute.commandPool.Destroy();
	m_vulkanResources->device.destroyDescriptorSetLayout(m_compute.descriptorSetLayout);
	m_vulkanResources->device.destroyPipeline(m_compute.pipeline);
	m_vulkanResources->device.destroyPipelineLayout(m_compute.pipelineLayout);
	m_vulkanResources->device.destroySemaphore(m_compute.semaphore);
	Vulkan::Cleanup();

	glfwDestroyWindow(m_window);

	glfwTerminate();
}