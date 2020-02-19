#include "OrreyVk.h"

void OrreyVk::run() {
	initWindow();
	initVulkan();
	mainLoop();
	cleanup();
}

void OrreyVk::initWindow() {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
}

void OrreyVk::initVulkan() {
	m_vulkan.Init();
}

void OrreyVk::mainLoop() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void OrreyVk::cleanup() {
	m_vulkan.Cleanup();

	glfwDestroyWindow(window);

	glfwTerminate();
}