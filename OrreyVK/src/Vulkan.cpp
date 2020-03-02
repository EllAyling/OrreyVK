#include "Vulkan.h"

void Vulkan::Init(GLFWwindow* window)
{
	m_vulkanResources.reset(new VulkanResources);
	CreateInstance();
	CreateSurface(window);
	CreateDevice();
	CreateSwapchain();
	CreateRenderpass();
	CreateFramebuffers();
	CreateCommandPool();

	m_sphere = SolidSphere(0.5, 20, 20);
	vk::DeviceMemory vertexStagingMemory, indexStagingMemory;
	vk::Buffer vertexStagingBuffer = CreateBuffer(m_sphere.GetVerticesSize(), vk::BufferUsageFlagBits::eTransferSrc, &vertexStagingMemory, m_sphere.GetVertices().data());
	vk::Buffer indexStagingBuffer = CreateBuffer(m_sphere.GetIndiciesSize(), vk::BufferUsageFlagBits::eTransferSrc, &indexStagingMemory, m_sphere.GetIndicies().data());
	m_bufferVertex.buffer = CreateBuffer(m_sphere.GetVerticesSize(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, &m_bufferVertex.memory, nullptr, vk::MemoryPropertyFlagBits::eDeviceLocal);
	m_bufferIndex.buffer = CreateBuffer(m_sphere.GetIndiciesSize(), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, &m_bufferIndex.memory, nullptr, vk::MemoryPropertyFlagBits::eDeviceLocal);

	CopyBuffer(vertexStagingBuffer, m_bufferVertex.buffer, m_sphere.GetVerticesSize());
	CopyBuffer(indexStagingBuffer, m_bufferIndex.buffer, m_sphere.GetIndiciesSize());

	m_graphics.ubo.projection = glm::perspective(glm::radians(45.0f), m_vulkanResources->swapchain.GetDimensions().width / (float)m_vulkanResources->swapchain.GetDimensions().height, 0.1f, 100.0f);
	m_graphics.ubo.projection[1][1] *= -1;
	m_graphics.ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	m_graphics.ubo.model = glm::mat4(1.0f);

	m_graphics.uniformBuffer = CreateBuffer(sizeof(m_graphics.ubo), vk::BufferUsageFlagBits::eUniformBuffer, &m_graphics.uniformBufferMemory, &m_graphics.ubo);

	m_vulkanResources->device.destroyBuffer(vertexStagingBuffer);
	m_vulkanResources->device.destroyBuffer(indexStagingBuffer);

	m_vulkanResources->device.freeMemory(vertexStagingMemory);
	m_vulkanResources->device.freeMemory(indexStagingMemory);

	CreateDescriptorPool();
	CreateDescriptorSetLayout();
	CreateDescriptorSet();

	CreateGraphicsPipelineLayout();
	CreateGraphicsPipeline();

	CreateCommandBuffers();

	CreateFencesAndSemaphores();

}

void Vulkan::RenderFrame()
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
	submitInfo.pCommandBuffers = &m_graphics.commandBuffers[imageIndex.value];

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_vulkanResources->semaphoreRender[m_frameID];

	m_vulkanResources->queueGraphics.submit(submitInfo, m_vulkanResources->fences[imageIndex.value]);

	vk::SwapchainKHR swapchains[] = { m_vulkanResources->swapchain.GetVkObject() };
	vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR(1, &m_vulkanResources->semaphoreRender[m_frameID], 1, swapchains, &imageIndex.value);
	
	m_vulkanResources->queueGraphics.presentKHR(presentInfo);

	m_frameID = (m_frameID + 1) % m_vulkanResources->swapchain.GetImageCount();
}

void Vulkan::Cleanup()
{
	m_vulkanResources->device.waitIdle();

	m_vulkanResources->device.destroyBuffer(m_bufferVertex.buffer);
	m_vulkanResources->device.destroyBuffer(m_bufferIndex.buffer);
	m_vulkanResources->device.destroyBuffer(m_graphics.uniformBuffer);

	m_vulkanResources->device.freeMemory(m_bufferVertex.memory);
	m_vulkanResources->device.freeMemory(m_bufferIndex.memory);
	m_vulkanResources->device.freeMemory(m_graphics.uniformBufferMemory);

	m_vulkanResources->device.destroyDescriptorSetLayout(m_graphics.descriptorSetLayout);
	m_vulkanResources->device.destroyDescriptorPool(m_vulkanResources->descriptorPool);

	for (int i = 0; i < m_vulkanResources->swapchain.GetImageCount(); i++)
	{
		m_vulkanResources->device.destroyFence(m_vulkanResources->fences[i]);
		m_vulkanResources->device.destroySemaphore(m_vulkanResources->semaphoreImageAquired[i]);
		m_vulkanResources->device.destroySemaphore(m_vulkanResources->semaphoreRender[i]);
	}
	m_graphics.commandPool.Destroy();
	m_vulkanResources->commandPoolTransfer.Destroy();
	m_vulkanResources->device.destroyPipeline(m_graphics.pipeline);
	m_vulkanResources->device.destroyPipelineLayout(m_graphics.pipelineLayout);
	for (auto& framebuffer : m_graphics.frameBuffers)
		m_vulkanResources->device.destroyFramebuffer(framebuffer);
	m_vulkanResources->device.destroyRenderPass(m_graphics.renderpass);
	m_vulkanResources->swapchain.Destroy();
	m_vulkanResources->device.destroy();
	m_vulkanResources->instance.destroySurfaceKHR(m_vulkanResources->surface);
	m_vulkanResources->instance.destroy();
}

void Vulkan::CreateInstance(VulkanTools::InstanceExtenstions extensionsRequested)
{
	vk::ApplicationInfo appInfo;
	appInfo.pApplicationName = "OrreyVK";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "OrreyVK";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

	vk::InstanceCreateInfo instanceInfo;
	instanceInfo.setPApplicationInfo(&appInfo);
	instanceInfo.setEnabledLayerCount(0);

	uint32_t apiVersion;
	vk::enumerateInstanceVersion(&apiVersion);
	spdlog::info("Vulkan Version: {}.{}.{}", VK_VERSION_MAJOR(apiVersion), VK_VERSION_MINOR(apiVersion), VK_VERSION_PATCH(apiVersion));
	appInfo.apiVersion = apiVersion;
	
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	//If we're not already requesting one of glfw required extensions, then add it here
	for (auto i = 0; i < glfwExtensionCount; ++i)
		if (!extensionsRequested.containsExtension(glfwExtensions[i]))
			extensionsRequested.addExtension(glfwExtensions[i]);
	
	uint32_t extensionCount = 0;
	std::vector<vk::ExtensionProperties> supportedExtensions = vk::enumerateInstanceExtensionProperties();

	if (supportedExtensions.size() == 0 && glfwExtensionCount != 0)
		throw std::runtime_error("Vulkan: We require instance extensions but no extensions are supported.");

	
	std::vector<const char*> enabledExtensions;
	if (supportedExtensions.size() != 0)
	{
		spdlog::info("Supported Extensions:");

		for (const auto& extension : supportedExtensions) {
			spdlog::info("\t{}, {}", extension.extensionName, extension.specVersion);
			bool foundExt = false;
			for (const auto& requestedExt : extensionsRequested.getExtensions()) {
				if (!strcmp(extension.extensionName, requestedExt.c_str()))
				{
					enabledExtensions.push_back(extension.extensionName);
					break;
				}
			}
		}

		instanceInfo.setEnabledExtensionCount(static_cast<uint32_t>(enabledExtensions.size()));
		instanceInfo.setPpEnabledExtensionNames(enabledExtensions.data());
		
		spdlog::info("Vulkan: Enabled Extensions:");
		for (const auto& extension : enabledExtensions)
			spdlog::info("\t{}", extension);
	}

#ifdef _DEBUG //if debug, enable validation layers
	std::vector<std::string> requestedLayers = { "VK_LAYER_KHRONOS_validation" };
	std::vector<vk::LayerProperties> supportedLayers = vk::enumerateInstanceLayerProperties();
	std::vector<const char*> enabledLayers;

	if (supportedLayers.size() != 0)
	{
		spdlog::info("Supported Layers:");
		for (const auto& layer : supportedLayers) {
			spdlog::info("\t{}, {}", layer.layerName, layer.implementationVersion);
			for (const auto& requestedLayer : requestedLayers) {
				if (!strcmp(layer.layerName, requestedLayer.c_str()))
					enabledLayers.push_back(layer.layerName);
			}
		}

		if (enabledLayers.size() != 0)
		{
			spdlog::info("Enabled Layers:");
			for (const auto& layer : enabledLayers)
				spdlog::info("\t{}", layer);
		}
		instanceInfo.ppEnabledLayerNames = enabledLayers.data();
		instanceInfo.enabledLayerCount = enabledLayers.size();
	}
#endif

	m_vulkanResources->instance = vk::createInstance(instanceInfo);

	uint32_t numPhysicalDevices = 0;
	std::vector<vk::PhysicalDevice> physicalDevices = m_vulkanResources->instance.enumeratePhysicalDevices();
	if(physicalDevices.size() == 0)
		throw std::runtime_error("Vulkan: No physical devices found");

	for (auto& physicalDevice : physicalDevices)
	{
		vk::PhysicalDeviceProperties properties;
		physicalDevice.getProperties(&properties);
		spdlog::info("Physical Device:");
		spdlog::info("\tDevice Name: {}", properties.deviceName);
		spdlog::info("\tDevice ID: {}", properties.deviceID);
		spdlog::info("\tDevice Type: {}", properties.deviceType);
		spdlog::info("\tVendor ID: {}", properties.vendorID);
		spdlog::info("\tAPI Version Support: {}.{}.{}", VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion), VK_VERSION_PATCH(properties.apiVersion));
		spdlog::info("\tDriver Version: {}", properties.driverVersion);

		if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			m_vulkanResources->physicalDevice = physicalDevice;
	}
	

	vk::PhysicalDeviceFeatures features;
	m_vulkanResources->physicalDevice.getFeatures(&features);
	
}

void Vulkan::CreateSurface(GLFWwindow* window)
{
#ifdef VK_USE_PLATFORM_WIN32_KHR
	VkSurfaceKHR surface;
	m_vulkanResources->window = window;
	glfwCreateWindowSurface(m_vulkanResources->instance, window, nullptr, &surface);
	m_vulkanResources->surface = surface;
#endif
}

void Vulkan::CreateDevice(VulkanTools::DeviceExtensions extensionsRequested)
{
	vk::DeviceCreateInfo deviceInfo;
	float priority = 1.0f;
	int queueFamilyID = 0;
	std::vector<vk::QueueFamilyProperties> queueFamilyProps = m_vulkanResources->physicalDevice.getQueueFamilyProperties();
	std::vector<vk::DeviceQueueCreateInfo> deviceQueues;
	for (auto& queueProps : queueFamilyProps)
	{
		std::vector<float> queuePriorites;
		spdlog::info("Queue Family: {}", queueFamilyID);
		spdlog::info("Queue Flags: {}{}{}{}",
			((queueProps.queueFlags & vk::QueueFlagBits::eGraphics)) ? "Graphics" : "",
			((queueProps.queueFlags & vk::QueueFlagBits::eCompute)) ? " | Compute" : "",
			((queueProps.queueFlags & vk::QueueFlagBits::eTransfer)) ? " | Transfer" : "",
			((queueProps.queueFlags & vk::QueueFlagBits::eSparseBinding)) ? " | Sparse Binding" : "",
			((queueProps.queueFlags & vk::QueueFlagBits::eProtected)) ? " | Protected" : ""
		);
		queueFamilyID++;
	}

	//Graphics
	int queueID = 0;
	queueFamilyID = 0;
	for (auto& queueProps : queueFamilyProps)
	{
		if (queueProps.queueFlags & vk::QueueFlagBits::eGraphics)
		{
			m_queueIDs.graphics.familyID = queueFamilyID;
			m_queueIDs.graphics.queueID = 0;
			vk::DeviceQueueCreateInfo createInfo = vk::DeviceQueueCreateInfo({}, queueFamilyID, 1, &priority);
			deviceQueues.push_back(createInfo);
			break;
		}
		queueFamilyID++;
	}

	//Compute
	queueFamilyID = 0;
	for (auto& queueProps : queueFamilyProps)
	{
		if((queueProps.queueFlags & vk::QueueFlagBits::eCompute) && !(queueProps.queueFlags & vk::QueueFlagBits::eGraphics))
		{
			m_queueIDs.compute.familyID = queueFamilyID;
			m_queueIDs.compute.queueID = 0;
			vk::DeviceQueueCreateInfo createInfo = vk::DeviceQueueCreateInfo({}, queueFamilyID, 1, &priority);
			deviceQueues.push_back(createInfo);
			break;
		}
		queueFamilyID++;
	}

	//If no dedicated compute queues
	queueFamilyID = 0;
	if(m_queueIDs.compute.familyID == -1)
	{
		for (auto& queueProps : queueFamilyProps)
		{
			if ((queueProps.queueFlags & vk::QueueFlagBits::eCompute))
			{
				m_queueIDs.compute.familyID = queueFamilyID;
				if(queueFamilyID == m_queueIDs.graphics.familyID)
					m_queueIDs.compute.queueID = ++queueID;
				else
					m_queueIDs.compute.queueID = 0;
			}
			queueFamilyID++;
		}
	}

	//Transfer
	queueFamilyID = 0;
	for (auto& queueProps : queueFamilyProps)
	{
		if ((queueProps.queueFlags & vk::QueueFlagBits::eTransfer) && !(queueProps.queueFlags & vk::QueueFlagBits::eGraphics) && !(queueProps.queueFlags & vk::QueueFlagBits::eCompute))
		{
			m_queueIDs.transfer.familyID = queueFamilyID;
			m_queueIDs.transfer.queueID = 0;
			vk::DeviceQueueCreateInfo createInfo = vk::DeviceQueueCreateInfo({}, queueFamilyID, 1, &priority);
			deviceQueues.push_back(createInfo);
			break;
		}
		queueFamilyID++;
	}

	//If no dedicated transfer queues
	queueFamilyID = 0;
	if (m_queueIDs.transfer.familyID == -1)
	{
		for (auto& queueProps : queueFamilyProps)
		{
			if ((queueProps.queueFlags & vk::QueueFlagBits::eTransfer))
			{
				m_queueIDs.transfer.familyID = queueFamilyID;
				if (queueFamilyID == m_queueIDs.graphics.familyID || queueFamilyID == m_queueIDs.compute.familyID)
					m_queueIDs.compute.queueID = ++queueID;
				
			}
			queueFamilyID++;
		}
	}

	//Present
	queueFamilyID = 0;
	for (auto& queueProps : queueFamilyProps)
	{
		if (m_vulkanResources->physicalDevice.getSurfaceSupportKHR(queueFamilyID, m_vulkanResources->surface))
		{
			m_queueIDs.present.familyID = queueFamilyID;
			m_queueIDs.present.queueID = ++queueID;
			if (queueFamilyID != m_queueIDs.graphics.familyID && queueFamilyID != m_queueIDs.compute.familyID && queueFamilyID != m_queueIDs.transfer.familyID)
			{
				vk::DeviceQueueCreateInfo createInfo = vk::DeviceQueueCreateInfo({}, queueFamilyID, 1, &priority);
				deviceQueues.push_back(createInfo);
				m_queueIDs.present.queueID = 0;
			}
			break;
		}
		queueFamilyID++;
	}

	deviceInfo.queueCreateInfoCount = deviceQueues.size();
	deviceInfo.pQueueCreateInfos = deviceQueues.data();

	vk::PhysicalDeviceFeatures features;
	m_vulkanResources->physicalDevice.getFeatures(&features);

	features.robustBufferAccess = false;
	deviceInfo.pEnabledFeatures = &features;

	std::vector<vk::ExtensionProperties> deviceExtProps = m_vulkanResources->physicalDevice.enumerateDeviceExtensionProperties();
	std::vector<const char*> enabledExtensions;

	spdlog::info("Supported Device Extensions:");
	for (auto& extension : deviceExtProps)
	{
		spdlog::info("\t{}, {}", extension.extensionName, extension.specVersion);
		bool foundExt = false;
		for (const auto& requestedExt : extensionsRequested.getExtensions()) {
			if (!strcmp(extension.extensionName, requestedExt.c_str()))
			{
				enabledExtensions.push_back(extension.extensionName);
				break;
			}
		}
	}

	deviceInfo.setEnabledExtensionCount(static_cast<uint32_t>(enabledExtensions.size()));
	deviceInfo.setPpEnabledExtensionNames(enabledExtensions.data());

	spdlog::info("Vulkan: Enabled Extensions:");
	for (const auto& extension : enabledExtensions)
		spdlog::info("\t{}", extension);

	m_vulkanResources->device = m_vulkanResources->physicalDevice.createDevice(deviceInfo);

	m_vulkanResources->queueGraphics = m_vulkanResources->device.getQueue(m_queueIDs.graphics.familyID, m_queueIDs.graphics.queueID);
	m_vulkanResources->queueCompute = m_vulkanResources->device.getQueue(m_queueIDs.compute.familyID, m_queueIDs.compute.queueID);
	m_vulkanResources->queueTransfer = m_vulkanResources->device.getQueue(m_queueIDs.transfer.familyID, m_queueIDs.transfer.queueID);
}

void Vulkan::CreateSwapchain()
{
	//Check swapchain support
	std::vector<vk::ExtensionProperties> deviceExtProps = m_vulkanResources->physicalDevice.enumerateDeviceExtensionProperties();
	bool swapchainSupport = false;
	for (auto& extension : deviceExtProps)
	{
		if (!strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
		{
			swapchainSupport = true;
		}
	}

	if(!swapchainSupport)
		throw std::runtime_error("Swapchain not supported.");

	vk::SurfaceCapabilitiesKHR surfaceCapabilities = m_vulkanResources->physicalDevice.getSurfaceCapabilitiesKHR(m_vulkanResources->surface);
	spdlog::info("Surface Information:");
	spdlog::info("\tMinimum Image count = {}", surfaceCapabilities.minImageCount);
	spdlog::info("\tMaximum Image count = {}", surfaceCapabilities.maxImageCount);
	spdlog::info("\tMaximum Image array layer count = {}", surfaceCapabilities.maxImageArrayLayers);
	spdlog::info("\tMinimum Image extent = {},{}", surfaceCapabilities.minImageExtent.width, surfaceCapabilities.minImageExtent.height);
	spdlog::info("\tMaximum Image extent = {},{}", surfaceCapabilities.maxImageExtent.width, surfaceCapabilities.maxImageExtent.height);

	vk::SwapchainCreateInfoKHR swapchainCreateInfo{};
	vk::ImageUsageFlags swapchainImageUseage = vk::ImageUsageFlagBits::eColorAttachment;

	if (surfaceCapabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc)
		swapchainImageUseage |= vk::ImageUsageFlagBits::eTransferSrc;
	
	swapchainCreateInfo.imageUsage = swapchainImageUseage;
	swapchainCreateInfo.imageExtent = surfaceCapabilities.maxImageExtent;

	std::vector<vk::SurfaceFormatKHR> surfaceFormats = m_vulkanResources->physicalDevice.getSurfaceFormatsKHR(m_vulkanResources->surface);
	vk::Format formatToUse = vk::Format::eUndefined;
	for (auto& format : surfaceFormats)
	{
		if (format.format == vk::Format::eB8G8R8A8Srgb)
		{
			formatToUse = format.format;
			break;
		}
	}

	if (formatToUse != vk::Format::eUndefined)
	{
		swapchainCreateInfo.imageFormat = formatToUse;
		swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
	}
	else
		throw std::runtime_error("Swapchain format not available.");

	std::vector<vk::PresentModeKHR> presentModes = m_vulkanResources->physicalDevice.getSurfacePresentModesKHR(m_vulkanResources->surface);
	vk::PresentModeKHR presentModeToUse = vk::PresentModeKHR::eFifo;
	swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
	for (auto& presentMode : presentModes)
	{
		//Use mailbox if available otherwise use fifo.
		if (presentMode == vk::PresentModeKHR::eMailbox)
		{
			presentModeToUse = vk::PresentModeKHR::eMailbox;
			swapchainCreateInfo.minImageCount = 3;
			break;
		}
	}

	swapchainCreateInfo.presentMode = presentModeToUse;
	swapchainCreateInfo.imageArrayLayers = 1;

	//If unique present queue
	if (m_queueIDs.present.familyID != m_queueIDs.graphics.familyID && m_queueIDs.present.familyID != m_queueIDs.compute.familyID && m_queueIDs.present.familyID != m_queueIDs.transfer.familyID)
	{
		swapchainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		swapchainCreateInfo.queueFamilyIndexCount = 2;
		uint32_t indicies[2] = { m_queueIDs.graphics.familyID, m_queueIDs.present.familyID };
		swapchainCreateInfo.pQueueFamilyIndices = indicies;
	}
	else
	{
		swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
	}

	swapchainCreateInfo.surface = m_vulkanResources->surface;

	m_vulkanResources->swapchain = VulkanSwapchain(m_vulkanResources->instance, m_vulkanResources->device, m_vulkanResources->physicalDevice, swapchainCreateInfo);
	vk::DeviceMemory depthImageMemory;
	m_vulkanResources->swapchain.SetDepthImage(CreateImage(vk::ImageType::e2D, vk::Format::eD32Sfloat,
		vk::Extent3D(swapchainCreateInfo.imageExtent, 1), vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageAspectFlagBits::eDepth, &depthImageMemory, vk::ImageCreateFlagBits(0), vk::SampleCountFlagBits::e1, vk::MemoryPropertyFlagBits::eDeviceLocal));
}

void Vulkan::CreateRenderpass()
{
	vk::AttachmentDescription colourAttachDesc = vk::AttachmentDescription({}, m_vulkanResources->swapchain.GetSwapchainFormat());
	colourAttachDesc.finalLayout = vk::ImageLayout::ePresentSrcKHR;
	colourAttachDesc.loadOp = vk::AttachmentLoadOp::eClear;
	colourAttachDesc.storeOp = vk::AttachmentStoreOp::eStore;
	colourAttachDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colourAttachDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

	vk::AttachmentDescription depthAttachDesc = vk::AttachmentDescription({}, vk::Format::eD32Sfloat);
	depthAttachDesc.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	depthAttachDesc.loadOp = vk::AttachmentLoadOp::eClear;
	depthAttachDesc.storeOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	depthAttachDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

	vk::AttachmentReference colourAttachRef = vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal);
	vk::AttachmentReference depthAttachRef = vk::AttachmentReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

	vk::SubpassDescription subpass = vk::SubpassDescription();
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colourAttachRef;
	subpass.pDepthStencilAttachment = &depthAttachRef;

	vk::SubpassDependency dependency = vk::SubpassDependency();
	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

	std::vector<vk::AttachmentDescription> attachments = { colourAttachDesc, depthAttachDesc };
	vk::RenderPassCreateInfo createInfo = vk::RenderPassCreateInfo({}, attachments.size(), attachments.data(), 1, &subpass, 1, &dependency);

	m_graphics.renderpass = m_vulkanResources->device.createRenderPass(createInfo);

}

void Vulkan::CreateFramebuffers()
{
	vk::FramebufferCreateInfo createInfo = vk::FramebufferCreateInfo({}, m_graphics.renderpass);
	vk::Extent2D dimensions = m_vulkanResources->swapchain.GetDimensions();
	createInfo.width = dimensions.width;
	createInfo.height = dimensions.height;
	createInfo.layers = 1;
	for (size_t i = 0; i < m_vulkanResources->swapchain.GetImageCount(); i++)
	{
		VulkanTools::ImagePair image = m_vulkanResources->swapchain.GetImages()[i];
		std::vector<vk::ImageView> attachments = { image.imageView, m_vulkanResources->swapchain.GetDepthImage().imageView };
		createInfo.attachmentCount = attachments.size();
		createInfo.pAttachments = attachments.data();

		m_graphics.frameBuffers.push_back(m_vulkanResources->device.createFramebuffer(createInfo));
	}
}

void Vulkan::CreateGraphicsPipelineLayout()
{
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo = vk::PipelineLayoutCreateInfo({}, 1, &m_graphics.descriptorSetLayout);
	m_graphics.pipelineLayout = m_vulkanResources->device.createPipelineLayout(pipelineLayoutInfo);
}

void Vulkan::CreateGraphicsPipeline()
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
	pipelineInfo.renderPass = m_graphics.renderpass;
	pipelineInfo.subpass = 0;
	
	m_graphics.pipeline = m_vulkanResources->device.createGraphicsPipeline(nullptr, pipelineInfo);

	m_vulkanResources->device.destroyShaderModule(vertShader);
	m_vulkanResources->device.destroyShaderModule(fragShader);
}

void Vulkan::CreateCommandPool()
{
	m_graphics.commandPool = VulkanCommandPool(m_vulkanResources->device, m_queueIDs.graphics.familyID, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	if (m_queueIDs.transfer.familyID == m_queueIDs.graphics.familyID)
		m_vulkanResources->commandPoolTransfer = m_graphics.commandPool;
	else
		m_vulkanResources->commandPoolTransfer = VulkanCommandPool(m_vulkanResources->device, m_queueIDs.transfer.familyID, vk::CommandPoolCreateFlagBits::eTransient);
}

void Vulkan::CreateCommandBuffers()
{
	uint32_t swapchainLength = m_vulkanResources->swapchain.GetImageCount();
	m_graphics.commandBuffers = m_graphics.commandPool.AllocateCommandBuffers(swapchainLength);
	for (size_t i = 0; i < swapchainLength; i++)
	{
		std::vector<vk::ClearValue> clearValues = {};
		clearValues.resize(2);
		clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
		clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0.0f);
		vk::RenderPassBeginInfo renderPassInfo = vk::RenderPassBeginInfo(m_graphics.renderpass, m_graphics.frameBuffers[i]);
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues.data();
		renderPassInfo.renderArea = vk::Rect2D({ 0, 0 }, m_vulkanResources->swapchain.GetDimensions());

		m_graphics.commandBuffers[i].begin(vk::CommandBufferBeginInfo());
		m_graphics.commandBuffers[i].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
		m_graphics.commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics.pipeline);
		m_graphics.commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphics.pipelineLayout, 0, 1, &m_graphics.descriptorSet, 0, nullptr);
		m_graphics.commandBuffers[i].bindVertexBuffers(0, m_bufferVertex.buffer, { 0 });
		m_graphics.commandBuffers[i].bindIndexBuffer(m_bufferIndex.buffer, { 0 }, vk::IndexType::eUint16);
		m_graphics.commandBuffers[i].drawIndexed(m_sphere.GetIndicies().size(), 1, 0, 0, 0);
		m_graphics.commandBuffers[i].endRenderPass();
		m_graphics.commandBuffers[i].end();
	}
}

void Vulkan::CreateFencesAndSemaphores()
{

	uint32_t swapchainLength = m_vulkanResources->swapchain.GetImageCount();
	for (size_t i = 0; i < swapchainLength; i++)
	{
		m_vulkanResources->semaphoreImageAquired.push_back(m_vulkanResources->device.createSemaphore(vk::SemaphoreCreateInfo()));
		m_vulkanResources->semaphoreRender.push_back(m_vulkanResources->device.createSemaphore(vk::SemaphoreCreateInfo()));
		m_vulkanResources->fences.push_back(m_vulkanResources->device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
	}
}

void Vulkan::CreateDescriptorPool()
{
	std::vector<vk::DescriptorPoolSize> poolSizes =
	{
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1)
	};

	vk::DescriptorPoolCreateInfo poolInfo = vk::DescriptorPoolCreateInfo({}, 1, poolSizes.size(), poolSizes.data());
	m_vulkanResources->descriptorPool = m_vulkanResources->device.createDescriptorPool(poolInfo);
}

void Vulkan::CreateDescriptorSetLayout()
{
	std::vector<vk::DescriptorSetLayoutBinding> layoutBindings =
	{
		vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex)
	};

	m_graphics.descriptorSetLayout = m_vulkanResources->device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, layoutBindings.size(), layoutBindings.data()));
}

void Vulkan::CreateDescriptorSet()
{
	vk::DescriptorSetAllocateInfo allocInfo = vk::DescriptorSetAllocateInfo(m_vulkanResources->descriptorPool, 1, &m_graphics.descriptorSetLayout);
	m_graphics.descriptorSet = m_vulkanResources->device.allocateDescriptorSets(allocInfo)[0];

	std::vector<vk::WriteDescriptorSet> writeSets =
	{
		vk::WriteDescriptorSet(m_graphics.descriptorSet, 2, 0, 1, vk::DescriptorType::eUniformBuffer, {}, &(vk::DescriptorBufferInfo(m_graphics.uniformBuffer, 0, VK_WHOLE_SIZE)))
	};

	m_vulkanResources->device.updateDescriptorSets(writeSets.size(), writeSets.data(), 0, nullptr);
}

vk::DeviceMemory Vulkan::AllocateAndBindMemory(vk::Image image, vk::MemoryPropertyFlags memoryFlags)
{
	vk::MemoryRequirements memRequirements = m_vulkanResources->device.getImageMemoryRequirements(image);
	vk::MemoryAllocateInfo allocInfo = vk::MemoryAllocateInfo(memRequirements.size, GetMemoryTypeIndex(memRequirements.memoryTypeBits, memoryFlags));
	vk::DeviceMemory memoryOut = m_vulkanResources->device.allocateMemory(allocInfo);
	m_vulkanResources->device.bindImageMemory(image, memoryOut, 0);
	return memoryOut;
}

vk::DeviceMemory Vulkan::AllocateAndBindMemory(vk::Buffer buffer, vk::MemoryPropertyFlags memoryFlags)
{
	vk::MemoryRequirements memRequirements = m_vulkanResources->device.getBufferMemoryRequirements(buffer);
	vk::MemoryAllocateInfo allocInfo = vk::MemoryAllocateInfo(memRequirements.size, GetMemoryTypeIndex(memRequirements.memoryTypeBits, memoryFlags));
	vk::DeviceMemory memoryOut = m_vulkanResources->device.allocateMemory(allocInfo);
	m_vulkanResources->device.bindBufferMemory(buffer, memoryOut, 0);
	return memoryOut;
}

uint32_t Vulkan::GetMemoryTypeIndex(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
	vk::PhysicalDeviceMemoryProperties memProps = m_vulkanResources->physicalDevice.getMemoryProperties();

	for (size_t i = 0; i < memProps.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
}

VulkanTools::ImagePair Vulkan::CreateImage(vk::ImageType imageType, vk::Format format, vk::Extent3D extent,
	vk::ImageUsageFlagBits usage, vk::ImageAspectFlagBits aspectFlags, vk::DeviceMemory* memoryOut, vk::ImageCreateFlagBits flags, vk::SampleCountFlagBits samples,
	vk::MemoryPropertyFlags memoryFlags, vk::SharingMode sharingMode,
	vk::ImageTiling tiling)
{
	vk::ImageCreateInfo createInfo = vk::ImageCreateInfo(flags, imageType, format, extent, 1, 1, samples, tiling, usage, sharingMode, 0, nullptr);

	vk::Image image = m_vulkanResources->device.createImage(createInfo);

	vk::DeviceMemory imageMemory = AllocateAndBindMemory(image, memoryFlags);
	if (memoryOut != nullptr)
		memoryOut = &imageMemory;

	vk::ImageViewCreateInfo viewCreateInfo = vk::ImageViewCreateInfo({}, image);
	viewCreateInfo.format = format;
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;

	vk::ImageViewType viewType = vk::ImageViewType::e1D;
	switch (imageType)
	{
	case vk::ImageType::e1D:
		viewCreateInfo.viewType = vk::ImageViewType::e1D;
		break;
	case vk::ImageType::e2D:
		viewCreateInfo.viewType = vk::ImageViewType::e2D;
		break;
	case vk::ImageType::e3D:
		viewCreateInfo.viewType = vk::ImageViewType::e3D;
		break;
	}

	vk::ImageView imageView = m_vulkanResources->device.createImageView(viewCreateInfo);

	return VulkanTools::ImagePair(image, imageView);
}

vk::Buffer Vulkan::CreateBuffer(uint32_t size, vk::BufferUsageFlags usage, vk::DeviceMemory* memoryOut, const void* data, vk::MemoryPropertyFlags memoryFlags, vk::SharingMode sharingMode)
{
	vk::BufferCreateInfo createInfo = vk::BufferCreateInfo({}, size, usage);
	vk::Buffer buffer = m_vulkanResources->device.createBuffer(createInfo);
	vk::DeviceMemory bufferMemory = AllocateAndBindMemory(buffer, memoryFlags);
	if (memoryOut != nullptr)
		memoryOut = &bufferMemory;

	if (data != nullptr)
	{
		void* bufData;
		bufData = m_vulkanResources->device.mapMemory(bufferMemory, 0, size);
		memcpy(bufData, data, size);
		m_vulkanResources->device.unmapMemory(bufferMemory);
	}

	return buffer;
}

void Vulkan::CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
{
	vk::CommandBuffer cmdBuffer = m_vulkanResources->commandPoolTransfer.AllocateCommandBuffer();
	cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	cmdBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
	cmdBuffer.end();

	vk::SubmitInfo submitInfo = vk::SubmitInfo();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	m_vulkanResources->queueTransfer.submit({ submitInfo }, {});
	m_vulkanResources->queueTransfer.waitIdle();
	m_vulkanResources->commandPoolTransfer.FreeCommandBuffers({ cmdBuffer });
}

vk::ShaderModule Vulkan::CompileShader(const std::string& filename, shaderc_shader_kind type)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open shader file.");
	}
	size_t fileSize = (size_t)file.tellg();
	std::string file_s(fileSize, '\0');
	file.seekg(0, std::ios::beg);
	file.read(&file_s[0], fileSize);
	file.close();

	std::string name = filename.substr(filename.find_last_of("/\\") + 1);

	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(file_s, type, name.c_str());
	
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		spdlog::error("Failed to compile shader file: {}", result.GetErrorMessage());
		throw std::runtime_error("Failed to compile shader file.");
	}
	
	std::vector<uint32_t> shaderData;
	shaderData.assign(result.cbegin(), result.cend());
	vk::ShaderModuleCreateInfo moduleCreateInfo = vk::ShaderModuleCreateInfo({}, shaderData.size() * sizeof(uint32_t), shaderData.data());

	spdlog::info("Compiled Shader: {}", filename);
	return m_vulkanResources->device.createShaderModule(moduleCreateInfo);
}
