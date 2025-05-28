#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

#include "vulkanrenderer.h"

GLFWwindow* g_window = nullptr;
VulkanRenderer vulkanRenderer;

void initWindow(const uint32_t width = 800, const uint32_t height = 400, const std::string& title = "Learn Vulkan")
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

	float angle = 0.0f;
	float deltaTime = 0.0f, lastTime = 0.0f;

	while (!glfwWindowShouldClose(g_window))
	{
		glfwPollEvents();

		float currentTime = glfwGetTime();
		deltaTime = currentTime - lastTime;
		lastTime = currentTime;

		angle += 10.0f * deltaTime;
		if (angle > 360.0f) angle -= 360.0f;

		vulkanRenderer.updateModel(glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f)));
		vulkanRenderer.draw();
	}

	vulkanRenderer.destroy();

	glfwDestroyWindow(g_window);
	glfwTerminate();

	return 0;
}