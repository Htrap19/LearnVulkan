#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <vector>
#include <set>
#include <array>
#include <cstring>
#include <algorithm>

#include "utilities.h"
#include "mesh.h"

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();

	int init(GLFWwindow* window);
	void destroy();

	void draw();
	void updateModel(glm::mat4 model);

private:
	// Create functions
	void createInstance();
	void setupDebugMessenger();
	void createSurface();
	void createLogicalDevice();
	void createSwapchain();
	void createRenderPass();
	void createDescriptorSetLayout();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSynchronisation();
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();

	void updateUniformBuffers(uint32_t imageIndex);

	// Record functions
	void recordCommands();

	// Get functions
	void getPhysicalDevice();

	// Support functions
	// -- Checker functions
	bool checkInstanceExtensionSupport(const std::vector<const char*>& checkExtensions);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkValidationLayerSupport();
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	// -- Getter functions
	std::vector<const char*> getRequiredExtensions() const;
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
	SwapchainDetails getSwapchainDetails(VkPhysicalDevice device);

	// -- Choose functions
	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR chooseBestPresentMode(const std::vector<VkPresentModeKHR>& presentationModes);
	VkExtent2D chooseSurfaceExtent(VkSurfaceCapabilitiesKHR surfaceCapabilities);

	// -- Create functions
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule createShaderModule(const std::vector<char>& shaderCode);

private:
	GLFWwindow* m_window = nullptr;

	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debugMessenger;
	VkSurfaceKHR m_surface;
	struct
	{
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} m_mainDevice;
	VkQueue m_graphicsQueue;
	VkQueue m_presentationQueue;
	VkSwapchainKHR m_swapchain;

	std::vector<SwapchainImage> m_swapchainImages;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;
	std::vector<VkCommandBuffer> m_commandBuffers;

	VkCommandPool m_graphicsCommandPool;

	VkPipelineLayout m_pipelineLayout;
	VkRenderPass m_renderPass;
	VkPipeline m_graphicsPipeline;

	VkDescriptorSetLayout m_descriptorSetLayout;

	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;

	std::vector<VkBuffer> m_uniformBuffers;
	std::vector<VkDeviceMemory> m_uniformBuffersMemory;

	VkFormat m_swapchainImageFormat;
	VkExtent2D m_swapchainImageExtent;

	std::vector<VkSemaphore> m_imageAvailable;
	std::vector<VkSemaphore> m_renderFinished;
	std::vector<VkFence> m_drawFences;
	int currentFrame = 0;

	std::vector<Mesh> m_meshes;
	struct MVP
	{
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
	} m_mvp;
};

