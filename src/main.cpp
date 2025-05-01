#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

#include "vulkanrenderer.h"

GLFWwindow* g_window = nullptr;
VulkanRenderer vulkanRenderer;

void initWindow(const uint32_t width = 740, const uint32_t height = 480, const std::string& title = "Learn Vulkan")
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	g_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
}

int main()
{
	glfwInit();

	initWindow();

	if (vulkanRenderer.init(g_window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	while (!glfwWindowShouldClose(g_window))
	{
		glfwPollEvents();
	}

	vulkanRenderer.destroy();

	glfwDestroyWindow(g_window);
	glfwTerminate();

	return 0;
}