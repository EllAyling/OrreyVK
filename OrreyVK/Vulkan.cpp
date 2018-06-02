#include "Vulkan.h"

void Vulkan::Init()
{
	CreateInstance();
}

void Vulkan::Cleanup()
{
	m_deviceResources.instance.destroy();
}

void Vulkan::CreateInstance()
{
	vk::ApplicationInfo appInfo	("OrreyVK", //App name
								VK_MAKE_VERSION(1, 0, 0), //App version 
								"OrreyVK", //Engine name
								VK_MAKE_VERSION(1, 0, 0), //Engine version 
								VK_API_VERSION_1_1); //Api version

	vk::InstanceCreateInfo instanceInfo;
	instanceInfo.setPApplicationInfo(&appInfo);
	instanceInfo.setEnabledLayerCount(0);

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	uint32_t extensionCount = 0;
	std::vector<vk::ExtensionProperties> supportedExtensions = vk::enumerateInstanceExtensionProperties();

	if (supportedExtensions.size() == 0 && glfwExtensionCount != 0)
		throw std::runtime_error("Vulkan: We require instance extensions but no extensions are supported.");

	if (supportedExtensions.size() != 0)
	{
		for (auto i = 0; i < glfwExtensionCount; ++i) {
			bool found = false;
			for (const auto& extension : supportedExtensions) {
				if (strcmp(glfwExtensions[i], extension.extensionName)) {
					found = true;
				}
			}
			if (!found) {
				throw std::runtime_error("Vulkan: Required instance extension not supported.");
			}
		}
	
		instanceInfo.setEnabledExtensionCount(glfwExtensionCount);
		instanceInfo.setPpEnabledExtensionNames(glfwExtensions);
		
		std::cout << "Vulkan: Enabled Extensions:" << std::endl;
		for (auto i = 0; i < glfwExtensionCount; ++i)
			std::cout << "\t" << glfwExtensions[i] << std::endl;
	}

	m_deviceResources.instance = vk::createInstance(instanceInfo);
}

void Vulkan::CreateDevice()
{

}

void Vulkan::CreateQueue()
{

}