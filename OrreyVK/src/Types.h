#pragma once
#ifndef TYPES_H
#define TYPES_H

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace VulkanTools {

	struct VulkanExtensions
	{
	private:
		std::vector<std::string> extensions;

	public:
		inline std::vector<std::string>& getExtensions()
		{
			return extensions;
		}

		inline uint32_t getExtensionCount() const
		{
			return extensions.size();
		}

		inline void addExtension(std::string extension)
		{
			extensions.emplace_back(extension);
		}

		inline void removeExtension(std::string extension)
		{
			extensions.erase(std::remove_if(extensions.begin(), extensions.end(), [extension](std::string extensions) {return extensions == extension; }));
		}

		inline bool containsExtension(std::string extension)
		{
			for (const auto& _extension : extensions) {
				if (_extension == extension)
					return true;
			}
			return false;
		}
	};

	struct InstanceExtenstions : VulkanExtensions
	{
		InstanceExtenstions()
		{
#ifdef VK_KHR_surface
			addExtension(VK_KHR_SURFACE_EXTENSION_NAME);
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
			addExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
#ifdef VK_KHR_get_physical_device_properties2
			addExtension(VK_KHR_GET_DISPLAY_PROPERTIES_2_EXTENSION_NAME);
#endif
#if _DEBUG
#ifdef VK_EXT_validation_features
			addExtension(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
			addExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
#endif
		}
	};

	struct DeviceExtensions : VulkanExtensions
	{
		DeviceExtensions()
		{
#ifdef VK_KHR_swapchain
			addExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#endif
		}
	};

	struct Queue
	{
		uint32_t familyID = -1;
		uint32_t queueID = -1;
	};

	struct QueueFamilies
	{
		Queue graphics;
		Queue compute;
		Queue transfer;
		Queue present;
	};

	struct ImagePair
	{
		vk::Image image;
		vk::ImageView imageView;

		ImagePair() { };
		ImagePair(vk::Image image, vk::ImageView imageView)
		{
			this->image = image;
			this->imageView = imageView;
		}
	};

	struct VertexInput
	{
		glm::vec3 pos;
		glm::vec4 colour;
	};

}
#endif