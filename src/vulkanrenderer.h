#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <cstring>

#include "utilities.h"

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();

	int init(GLFWwindow* window);
	void destroy();

private:
	// Create functions
	void createInstance();
	void createLogicalDevice();

	// Get functions
	void getPhysicalDevice();

	// Support functions
	// -- Checker functions
	bool checkInstanceExtensionSupport(const char* const* checkExtensions, uint32_t checkExtensionCount);
	bool checkDeviceSuitable(VkPhysicalDevice device);

	// -- Getter functions
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);

private:
	GLFWwindow* m_window = nullptr;

	VkInstance m_instance;
	struct
	{
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} m_mainDevice;
	VkQueue m_graphicsQueue;
};

