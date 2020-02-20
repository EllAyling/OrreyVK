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

	m_window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
}

void OrreyVk::initVulkan() {
	m_vulkan.Init(m_window);
}

void OrreyVk::mainLoop() {
	while (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();
		m_vulkan.RenderFrame();
	}
}

void OrreyVk::cleanup() {
	m_vulkan.Cleanup();

	glfwDestroyWindow(m_window);

	glfwTerminate();
}