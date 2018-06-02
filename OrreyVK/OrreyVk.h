#pragma once

#ifndef ORREYVK_H
#define ORREYVK_H

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include "Vulkan.h"


class OrreyVk {
public:
	void run();
	void initWindow();
	void initVulkan();
	void mainLoop();
	void cleanup();

private:
	GLFWwindow * window;
	Vulkan m_vulkan;

};

#endif // !ORREYVK_H