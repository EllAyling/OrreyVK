#include "VulkanSwapChain.h"
namespace vko {

	std::vector<VulkanTools::ImageResources> VulkanSwapchain::GetImages()
	{
		return swapchainImages;
	}

	void VulkanSwapchain::Destroy()
	{

		device.destroyImageView(depthImage.imageView);
		device.destroyImage(depthImage.image);

		for (auto& images : swapchainImages)
		{
			device.destroyImageView(images.imageView);
		}

		device.destroySwapchainKHR(swapchain);
	}
}