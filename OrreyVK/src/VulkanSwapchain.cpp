#include "VulkanSwapChain.h"

std::vector<VulkanTools::ImagePair> VulkanSwapchain::GetImages()
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
