#include "Vulkan.h"


VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData, void * pUserData)
{
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		spdlog::error("{}", pCallbackData->pMessage);
	}
	else {
		spdlog::warn("{}", pCallbackData->pMessage);
	}

	return VK_FALSE;
}

void Vulkan::InitVulkan(GLFWwindow* window)
{
	m_vulkanResources.reset(new VulkanResources);
	CreateInstance();
#if _DEBUG
	CreateDebugging();
#endif
	CreateSurface(window);
	CreateDevice();
	CreateSwapchain();
	CreateRenderpass();
	CreateFramebuffers();
	CreateCommandPool();
	CreateFencesAndSemaphores();
}

void Vulkan::Cleanup()
{
	m_vulkanResources->device.waitIdle();

	for (int i = 0; i < m_vulkanResources->swapchain.GetImageCount(); i++)
	{
		m_vulkanResources->device.destroySemaphore(m_vulkanResources->semaphoreImageAquired[i]);
		m_vulkanResources->device.destroySemaphore(m_vulkanResources->semaphoreRender[i]);
	}

	m_vulkanResources->commandPool.Destroy();
	m_vulkanResources->commandPoolTransfer.Destroy();
	m_vulkanResources->device.destroyDescriptorPool(m_vulkanResources->descriptorPool);
	
	for (auto& framebuffer : m_vulkanResources->frameBuffers)
		m_vulkanResources->device.destroyFramebuffer(framebuffer);
	m_vulkanResources->device.destroyRenderPass(m_vulkanResources->renderpass);
	m_vulkanResources->swapchain.Destroy();
	m_vulkanResources->device.destroy();
	m_vulkanResources->instance.destroySurfaceKHR(m_vulkanResources->surface);

	if (m_debug.debugUtilsMessenger != VK_NULL_HANDLE)
		m_debug.vkDestroyDebugUtilsMessengerEXT(m_vulkanResources->instance, m_debug.debugUtilsMessenger, nullptr);

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
		{
			m_vulkanResources->physicalDevice = physicalDevice;
			vk::SampleCountFlags maxSamples = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
			auto getSampleCount = [maxSamples] {
				if (maxSamples & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
				if (maxSamples & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
				if (maxSamples & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
				if (maxSamples & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
				if (maxSamples & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
				if (maxSamples & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }
				return vk::SampleCountFlagBits::e1;
			};
			m_msaaSamples = getSampleCount();
			break;
		}
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
			m_queueIDs.graphics.minImageTransferGranularity = queueProps.minImageTransferGranularity;
			m_queueIDs.graphics.timestampValidBits = queueProps.timestampValidBits;
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
			m_queueIDs.compute.minImageTransferGranularity = queueProps.minImageTransferGranularity;
			m_queueIDs.compute.timestampValidBits = queueProps.timestampValidBits;
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
			m_queueIDs.transfer.minImageTransferGranularity = queueProps.minImageTransferGranularity;
			m_queueIDs.transfer.timestampValidBits = queueProps.timestampValidBits;
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

	vk::PhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
	vk::PhysicalDeviceFeatures2 features2;
	features2.setPNext(&indexingFeatures);
	m_vulkanResources->physicalDevice.getFeatures2(&features2);
	deviceInfo.setPNext(&indexingFeatures);

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
	vk::ImageUsageFlags swapchainImageUsage = vk::ImageUsageFlagBits::eColorAttachment;

	if (surfaceCapabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc)
		swapchainImageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
	
	swapchainCreateInfo.imageUsage = swapchainImageUsage;
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

	m_vulkanResources->swapchain = vko::VulkanSwapchain(m_vulkanResources->instance, m_vulkanResources->device, m_vulkanResources->physicalDevice, swapchainCreateInfo);
	vk::DeviceMemory depthImageMemory;
	vko::Image depthImage = CreateImage(vk::ImageType::e2D, vk::Format::eD32Sfloat,
		vk::Extent3D(swapchainCreateInfo.imageExtent, 1), vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageAspectFlagBits::eDepth,
		vk::ImageCreateFlagBits(0), m_msaaSamples, vk::MemoryPropertyFlagBits::eDeviceLocal);
	m_vulkanResources->swapchain.SetDepthImage(VulkanTools::ImageResources(depthImage.image, depthImage.imageView, depthImage.memory));

	vko::Image multiSampleImage = CreateImage(vk::ImageType::e2D, formatToUse,
		vk::Extent3D(swapchainCreateInfo.imageExtent, 1), vk::ImageUsageFlagBits::eColorAttachment, vk::ImageAspectFlagBits::eColor,
		vk::ImageCreateFlagBits(0), m_msaaSamples, vk::MemoryPropertyFlagBits::eDeviceLocal);
	m_vulkanResources->swapchain.SetMultiSampleImage(VulkanTools::ImageResources(multiSampleImage.image, multiSampleImage.imageView, multiSampleImage.memory));
}

void Vulkan::CreateRenderpass()
{
	vk::AttachmentDescription colourAttachDesc = vk::AttachmentDescription({}, m_vulkanResources->swapchain.GetSwapchainFormat());
	colourAttachDesc.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
	colourAttachDesc.loadOp = vk::AttachmentLoadOp::eClear;
	colourAttachDesc.storeOp = vk::AttachmentStoreOp::eStore;
	colourAttachDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colourAttachDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	colourAttachDesc.samples = m_msaaSamples;

	vk::AttachmentDescription depthAttachDesc = vk::AttachmentDescription({}, vk::Format::eD32Sfloat);
	depthAttachDesc.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	depthAttachDesc.loadOp = vk::AttachmentLoadOp::eClear;
	depthAttachDesc.storeOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	depthAttachDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachDesc.samples = m_msaaSamples;

	vk::AttachmentDescription colourAttachResolveDesc = vk::AttachmentDescription({}, m_vulkanResources->swapchain.GetSwapchainFormat());
	colourAttachResolveDesc.finalLayout = vk::ImageLayout::ePresentSrcKHR;
	colourAttachResolveDesc.loadOp = vk::AttachmentLoadOp::eDontCare;
	colourAttachResolveDesc.storeOp = vk::AttachmentStoreOp::eStore;
	colourAttachResolveDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colourAttachResolveDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	colourAttachResolveDesc.samples = vk::SampleCountFlagBits::e1;

	vk::AttachmentReference colourAttachRef = vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal);
	vk::AttachmentReference depthAttachRef = vk::AttachmentReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
	vk::AttachmentReference colourAttachResolveRef = vk::AttachmentReference(2, vk::ImageLayout::eColorAttachmentOptimal);

	vk::SubpassDescription subpass = vk::SubpassDescription();
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colourAttachRef;
	subpass.pDepthStencilAttachment = &depthAttachRef;
	subpass.pResolveAttachments = &colourAttachResolveRef;

	vk::SubpassDependency dependency = vk::SubpassDependency();
	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

	std::vector<vk::AttachmentDescription> attachments = { colourAttachDesc, depthAttachDesc, colourAttachResolveDesc };
	vk::RenderPassCreateInfo createInfo = vk::RenderPassCreateInfo({}, attachments.size(), attachments.data(), 1, &subpass, 1, &dependency);

	m_vulkanResources->renderpass = m_vulkanResources->device.createRenderPass(createInfo);

}

void Vulkan::CreateFramebuffers()
{
	vk::FramebufferCreateInfo createInfo = vk::FramebufferCreateInfo({}, m_vulkanResources->renderpass);
	vk::Extent2D dimensions = m_vulkanResources->swapchain.GetDimensions();
	createInfo.width = dimensions.width;
	createInfo.height = dimensions.height;
	createInfo.layers = 1;
	for (size_t i = 0; i < m_vulkanResources->swapchain.GetImageCount(); i++)
	{
		VulkanTools::ImageResources image = m_vulkanResources->swapchain.GetImages()[i];
		std::vector<vk::ImageView> attachments = { m_vulkanResources->swapchain.GetMultiSampleImage().imageView, m_vulkanResources->swapchain.GetDepthImage().imageView, image.imageView  };
		createInfo.attachmentCount = attachments.size();
		createInfo.pAttachments = attachments.data();

		m_vulkanResources->frameBuffers.push_back(m_vulkanResources->device.createFramebuffer(createInfo));
	}
}

void Vulkan::CreateDebugging()
{
	m_debug.vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)m_vulkanResources->instance.getProcAddr("vkCreateDebugUtilsMessengerEXT");
	m_debug.vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)m_vulkanResources->instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT");

	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
	debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugUtilsMessengerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	debugUtilsMessengerCI.pfnUserCallback = debugUtilsMessengerCallback;

	m_debug.vkCreateDebugUtilsMessengerEXT(m_vulkanResources->instance, &debugUtilsMessengerCI, nullptr, &m_debug.debugUtilsMessenger);
}

void Vulkan::CreateCommandPool()
{
	m_vulkanResources->commandPool = vko::VulkanCommandPool(m_vulkanResources->device, m_queueIDs.graphics.familyID, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	if (m_queueIDs.transfer.familyID == m_queueIDs.graphics.familyID)
		m_vulkanResources->commandPoolTransfer = m_vulkanResources->commandPool;
	else
		m_vulkanResources->commandPoolTransfer = vko::VulkanCommandPool(m_vulkanResources->device, m_queueIDs.transfer.familyID, vk::CommandPoolCreateFlagBits::eTransient);
}

void Vulkan::CreateFencesAndSemaphores()
{

	uint32_t swapchainLength = m_vulkanResources->swapchain.GetImageCount();
	for (size_t i = 0; i < swapchainLength; i++)
	{
		m_vulkanResources->semaphoreImageAquired.push_back(m_vulkanResources->device.createSemaphore(vk::SemaphoreCreateInfo()));
		m_vulkanResources->semaphoreRender.push_back(m_vulkanResources->device.createSemaphore(vk::SemaphoreCreateInfo()));
	}
}

vk::DeviceMemory Vulkan::AllocateAndBindMemory(vk::Image image, vk::MemoryPropertyFlags memoryFlags, vk::MemoryAllocateInfo *allocInfoOut)
{
	vk::MemoryRequirements memRequirements = m_vulkanResources->device.getImageMemoryRequirements(image);
	vk::MemoryAllocateInfo allocInfo = vk::MemoryAllocateInfo(memRequirements.size, GetMemoryTypeIndex(memRequirements.memoryTypeBits, memoryFlags));
	vk::DeviceMemory memoryOut = m_vulkanResources->device.allocateMemory(allocInfo);
	m_vulkanResources->device.bindImageMemory(image, memoryOut, 0);
	if (allocInfoOut != nullptr)
		*allocInfoOut = allocInfo;
	return memoryOut;
}

vk::DeviceMemory Vulkan::AllocateAndBindMemory(vk::Buffer buffer, vk::MemoryPropertyFlags memoryFlags, vk::MemoryAllocateInfo *allocInfoOut)
{
	vk::MemoryRequirements memRequirements = m_vulkanResources->device.getBufferMemoryRequirements(buffer);
	vk::MemoryAllocateInfo allocInfo = vk::MemoryAllocateInfo(memRequirements.size, GetMemoryTypeIndex(memRequirements.memoryTypeBits, memoryFlags));
	vk::DeviceMemory memoryOut = m_vulkanResources->device.allocateMemory(allocInfo);
	m_vulkanResources->device.bindBufferMemory(buffer, memoryOut, 0);
	if (allocInfoOut != nullptr)
		*allocInfoOut = allocInfo;
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

vko::Image Vulkan::CreateImage(vk::ImageType imageType, vk::Format format, vk::Extent3D extent,
	vk::ImageUsageFlags usage, vk::ImageAspectFlagBits aspectFlags, vk::ImageCreateFlags flags, vk::SampleCountFlagBits samples,
	vk::MemoryPropertyFlags memoryFlags, int mipLevels, int layerCount, vk::SharingMode sharingMode,
	vk::ImageTiling tiling)
{
	vk::ImageCreateInfo createInfo = vk::ImageCreateInfo(flags, imageType, format, extent, mipLevels, layerCount, samples, tiling, usage, sharingMode, 0, nullptr);

	vk::Image image = m_vulkanResources->device.createImage(createInfo);

	vk::MemoryAllocateInfo allocInfo;
	vk::DeviceMemory imageMemory = AllocateAndBindMemory(image, memoryFlags, &allocInfo);

	vk::ImageViewCreateInfo viewCreateInfo = vk::ImageViewCreateInfo({}, image);
	viewCreateInfo.format = format;
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.levelCount = mipLevels;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = layerCount;

	vk::ImageViewType viewType = vk::ImageViewType::e1D;
	switch (imageType)
	{
	case vk::ImageType::e1D:
		viewCreateInfo.viewType = vk::ImageViewType::e1D;
		break;
	case vk::ImageType::e2D:
		if (layerCount > 1)
			viewCreateInfo.viewType = vk::ImageViewType::e2DArray;
		else
			viewCreateInfo.viewType = vk::ImageViewType::e2D;
		break;
	case vk::ImageType::e3D:
		viewCreateInfo.viewType = vk::ImageViewType::e3D;
		break;
	}

	vk::ImageView imageView = m_vulkanResources->device.createImageView(viewCreateInfo);

	return vko::Image(m_vulkanResources->device, m_vulkanResources->physicalDevice, image, imageView, imageMemory, allocInfo.allocationSize, extent, mipLevels, vk::ImageLayout::eUndefined, usage, memoryFlags);
}

void Vulkan::GenerateMipmaps(vko::Image image, vk::Filter mipMapFiltering, uint32_t mipLevels, uint32_t arrayLevels)
{
	vk::CommandBuffer cmdBuffer = m_vulkanResources->commandPool.AllocateCommandBuffer();
	cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	vk::ImageMemoryBarrier imgMemBarrier;

	for (int i = 0; i < arrayLevels; i++)
	{
		int32_t mipWidth = image.extent.width;
		int32_t mipHeight = image.extent.height;
		int32_t mipDepth = image.extent.depth;
		for (int j = 1; j < mipLevels; j++)
		{
			std::array<vk::Offset3D, 2> srcOffets = { vk::Offset3D(0, 0, 0), vk::Offset3D(mipWidth, mipHeight, mipDepth) };
			std::array<vk::Offset3D, 2> dstOffets = { vk::Offset3D(0, 0, 0), vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, mipDepth > 1 ? mipDepth / 2 : 1) };

			vk::ImageBlit blitRegion = vk::ImageBlit(
				vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, j - 1, i, 1), 
				srcOffets,
				vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, j, i, 1), 
				dstOffets);

			InsertImageMemoryBarrier(cmdBuffer, image.image, {}, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
				vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, j, 1, i, 1));

			cmdBuffer.blitImage(image.image, vk::ImageLayout::eTransferSrcOptimal, image.image, vk::ImageLayout::eTransferDstOptimal, 1, &blitRegion, mipMapFiltering);

			InsertImageMemoryBarrier(cmdBuffer, image.image, vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
				vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, j, 1, i, 1));

			if (mipWidth > 1)
				mipWidth /= 2;
			if (mipHeight > 1)
				mipHeight /= 2;
			if (mipDepth > 1)
				mipDepth /= 2;
		}
	}

	InsertImageMemoryBarrier(cmdBuffer, image.image, vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, arrayLevels));

	cmdBuffer.end();

	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;
	vk::Fence fence = m_vulkanResources->device.createFence(vk::FenceCreateInfo());

	m_vulkanResources->queueGraphics.submit(submitInfo, fence);
	m_vulkanResources->device.waitForFences(fence, true, UINT64_MAX);
	m_vulkanResources->device.destroyFence(fence);

	m_vulkanResources->commandPool.FreeCommandBuffers(cmdBuffer);
}

vko::Buffer Vulkan::CreateBuffer(uint32_t size, vk::BufferUsageFlags usage, const void* data, vk::MemoryPropertyFlags memoryFlags, vk::SharingMode sharingMode)
{
	vk::BufferCreateInfo createInfo = vk::BufferCreateInfo({}, size, usage);
	vk::Buffer buffer = m_vulkanResources->device.createBuffer(createInfo);
	vk::MemoryAllocateInfo allocInfo;
	vk::DeviceMemory bufferMemory = AllocateAndBindMemory(buffer, memoryFlags, &allocInfo);

	if (data != nullptr)
	{
		void* bufData;
		bufData = m_vulkanResources->device.mapMemory(bufferMemory, 0, allocInfo.allocationSize);
		memcpy(bufData, data, size);
		m_vulkanResources->device.unmapMemory(bufferMemory);
	}

	return vko::Buffer(m_vulkanResources->device, bufferMemory, buffer, size, usage, memoryFlags);
}

void Vulkan::CopyBuffer(vko::Buffer srcBuffer, vko::Buffer dstBuffer, vk::DeviceSize size)
{
	vk::CommandBuffer cmdBuffer = m_vulkanResources->commandPool.AllocateCommandBuffer();
	cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	cmdBuffer.copyBuffer(srcBuffer.buffer, dstBuffer.buffer, vk::BufferCopy(0, 0, size));
	cmdBuffer.end();

	vk::SubmitInfo submitInfo = vk::SubmitInfo();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	m_vulkanResources->queueGraphics.submit({ submitInfo }, {});
	m_vulkanResources->queueGraphics.waitIdle();
	m_vulkanResources->commandPool.FreeCommandBuffers({ cmdBuffer });
}

vk::ShaderModule Vulkan::CompileShader(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open shader file.");
	}
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), fileSize);
	file.close();

	vk::ShaderModuleCreateInfo moduleCreateInfo = vk::ShaderModuleCreateInfo({}, buffer.size(), reinterpret_cast<const uint32_t*>(buffer.data()));

	spdlog::info("Compiled Shader: {}", filename);
	return m_vulkanResources->device.createShaderModule(moduleCreateInfo);
}

vko::Image Vulkan::CreateTexture(vk::ImageType imageType, vk::Format format, const char * filePath, vk::ImageUsageFlags usage, bool generateMipMaps, vk::Filter mipMapFilter)
{
	int width;
	int height;
	int channels;

	stbi_set_flip_vertically_on_load(false);
	void* data = stbi_load(filePath, &width, &height, &channels, 4);

	int memorySize = width * height * sizeof(unsigned char) * 4;
	int mipLevels = 1;

	vk::FormatProperties props = m_vulkanResources->physicalDevice.getFormatProperties(format);
	assert(props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc);
	assert(props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst);

	if (generateMipMaps)
	{
		if (!(usage & vk::ImageUsageFlagBits::eTransferSrc))
			usage |= vk::ImageUsageFlagBits::eTransferSrc;

		if (!(usage & vk::ImageUsageFlagBits::eTransferDst))
			usage |= vk::ImageUsageFlagBits::eTransferDst;

		mipLevels = static_cast<uint32_t>(std::floor(log2(std::max(width, height)))) + 1;
	}

	vk::DeviceMemory imageMemory;
	vko::Image textureImage = CreateImage(imageType, format, vk::Extent3D(width, height, 1), usage, vk::ImageAspectFlagBits::eColor, {}, vk::SampleCountFlagBits::e1, vk::MemoryPropertyFlagBits::eDeviceLocal, mipLevels);
	vko::Buffer imageBuffer = CreateBuffer(memorySize, vk::BufferUsageFlagBits::eTransferSrc, data);
	stbi_image_free(data);

	vk::CommandBuffer cmdBuffer = m_vulkanResources->commandPoolTransfer.AllocateCommandBuffer();
	cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	InsertImageMemoryBarrier(cmdBuffer, textureImage.image, {}, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

	vk::BufferImageCopy regions = vk::BufferImageCopy(0, width, height, 
		vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), 
		vk::Offset3D(0, 0, 0), vk::Extent3D(width, height, 1));

	cmdBuffer.copyBufferToImage(imageBuffer.buffer, textureImage.image, vk::ImageLayout::eTransferDstOptimal, regions);
		
	InsertImageMemoryBarrier(cmdBuffer, textureImage.image, 
		vk::AccessFlagBits::eTransferWrite, mipLevels == 1 ? vk::AccessFlagBits::eShaderRead : vk::AccessFlagBits::eTransferRead, 
		vk::ImageLayout::eTransferDstOptimal, mipLevels == 1 ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eTransferSrcOptimal,
		vk::PipelineStageFlagBits::eTransfer, mipLevels == 1 ? vk::PipelineStageFlagBits::eFragmentShader : vk::PipelineStageFlagBits::eTransfer, 
		vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

	cmdBuffer.end();

	vk::SubmitInfo submitInfo = vk::SubmitInfo();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;
	vk::Fence fence = m_vulkanResources->device.createFence(vk::FenceCreateInfo());

	m_vulkanResources->queueTransfer.submit({ submitInfo }, fence);
	m_vulkanResources->device.waitForFences(fence, true, UINT64_MAX);
	m_vulkanResources->device.destroyFence(fence);

	if(mipLevels > 1)
		GenerateMipmaps(textureImage, mipMapFilter, mipLevels, 1);
	   
	m_vulkanResources->commandPoolTransfer.FreeCommandBuffers({ cmdBuffer });

	imageBuffer.Destroy();

	textureImage.SetImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

	return textureImage;
}

vko::Image Vulkan::Create2DTextureArray(vk::Format format, std::vector<const char*> filePaths, vk::ImageUsageFlags usage, bool generateMipMaps, vk::Filter mipMapFilter)
{
	stbi_set_flip_vertically_on_load(false);
	struct ImageInfo {
		int size;
		int width;
		int height;
	};

	std::vector<ImageInfo> imageInfo;
	int width;
	int height;
	int channels;
	int totalMemorySize = 0;
	
	for (auto path : filePaths)
	{
		stbi_info(path, &width, &height, &channels);
		ImageInfo imgInfo = { 
			width * height * sizeof(unsigned char) * 4,
			width, height
			};
		imageInfo.push_back(imgInfo);
		totalMemorySize += imageInfo.back().size;
	}

	int mipLevels = 1;
	if (generateMipMaps)
	{
		if (!(usage & vk::ImageUsageFlagBits::eTransferSrc))
			usage |= vk::ImageUsageFlagBits::eTransferSrc;

		mipLevels = static_cast<uint32_t>(std::floor(log2(std::max(width, height)))) + 1;
	}

	vk::BufferCreateInfo createInfo = vk::BufferCreateInfo({}, totalMemorySize, vk::BufferUsageFlagBits::eTransferSrc);
	vk::Buffer buffer = m_vulkanResources->device.createBuffer(createInfo);
	vk::DeviceMemory bufferMemory = AllocateAndBindMemory(buffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	int offset = 0;
	for (int i = 0; i < imageInfo.size(); i++)
	{
		void* data = stbi_load(filePaths[i], &width, &height, &channels, 4);
		void* bufData;
		bufData = m_vulkanResources->device.mapMemory(bufferMemory, offset, imageInfo[i].size);
		memcpy(bufData, data, imageInfo[i].size);
		m_vulkanResources->device.unmapMemory(bufferMemory);
		offset += imageInfo[i].size;
		stbi_image_free(data);
	}

	vko::Buffer imageBuffer = vko::Buffer(m_vulkanResources->device, bufferMemory, buffer, totalMemorySize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	vk::DeviceMemory imageMemory;
	vko::Image textureImage = CreateImage(vk::ImageType::e2D, format, vk::Extent3D(width, height, 1), usage, vk::ImageAspectFlagBits::eColor,
		{}, vk::SampleCountFlagBits::e1, vk::MemoryPropertyFlagBits::eDeviceLocal, mipLevels, imageInfo.size());

	vk::CommandBuffer cmdBuffer = m_vulkanResources->commandPoolTransfer.AllocateCommandBuffer();
	cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	InsertImageMemoryBarrier(cmdBuffer, textureImage.image, {}, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
		vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, imageInfo.size()));

	std::vector<vk::BufferImageCopy> regions;
	offset = 0;
	for (int i = 0; i < imageInfo.size(); i++)
	{
		regions.emplace_back(vk::BufferImageCopy(offset, imageInfo[i].width, imageInfo[i].height,
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, i, 1),
			vk::Offset3D(0, 0, 0), vk::Extent3D(width, height, 1)));
		offset += imageInfo[i].size;
	}

	cmdBuffer.copyBufferToImage(imageBuffer.buffer, textureImage.image, vk::ImageLayout::eTransferDstOptimal, regions);

	InsertImageMemoryBarrier(cmdBuffer, textureImage.image,
		vk::AccessFlagBits::eTransferWrite, mipLevels == 1 ? vk::AccessFlagBits::eShaderRead : vk::AccessFlagBits::eTransferRead,
		vk::ImageLayout::eTransferDstOptimal, mipLevels == 1 ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eTransferSrcOptimal,
		vk::PipelineStageFlagBits::eTransfer, mipLevels == 1 ? vk::PipelineStageFlagBits::eFragmentShader : vk::PipelineStageFlagBits::eTransfer,
		vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, imageInfo.size()));

	cmdBuffer.end();

	vk::SubmitInfo submitInfo = vk::SubmitInfo();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;
	vk::Fence fence = m_vulkanResources->device.createFence(vk::FenceCreateInfo());

	m_vulkanResources->queueTransfer.submit({ submitInfo }, fence);
	m_vulkanResources->device.waitForFences(fence, true, UINT64_MAX);
	m_vulkanResources->device.destroyFence(fence);

	if (mipLevels > 1)
		GenerateMipmaps(textureImage, mipMapFilter, mipLevels, imageInfo.size());
	
	m_vulkanResources->commandPoolTransfer.FreeCommandBuffers({ cmdBuffer });

	imageBuffer.Destroy();

	textureImage.SetImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
	
	return textureImage;
}

void Vulkan::InsertImageMemoryBarrier(vk::CommandBuffer cmdbuffer, vk::Image image, vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask, vk::ImageSubresourceRange subresourceRange)
{
	vk::ImageMemoryBarrier imageMemoryBarrier = vk::ImageMemoryBarrier();
	imageMemoryBarrier.srcAccessMask = srcAccessMask;
	imageMemoryBarrier.dstAccessMask = dstAccessMask;
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = subresourceRange;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	cmdbuffer.pipelineBarrier(
		srcStageMask,
		dstStageMask,
		{},
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier);
}

void Vulkan::InsertBufferMemoryBarrier(vk::CommandBuffer cmdbuffer, vko::Buffer buffer, vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask, vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask, uint32_t srcQueue, uint32_t dstQueue)
{
	vk::BufferMemoryBarrier barrier = vk::BufferMemoryBarrier();
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.buffer = buffer.buffer;
	barrier.offset = 0;
	barrier.size = buffer.size;
	barrier.srcQueueFamilyIndex = srcQueue;
	barrier.dstQueueFamilyIndex = dstQueue;

	cmdbuffer.pipelineBarrier(
		srcStageMask,
		dstStageMask,
		{},
		0, nullptr,
		1, &barrier,
		0, nullptr);
}
