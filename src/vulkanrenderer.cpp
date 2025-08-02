#include "vulkanrenderer.h"

#include <iostream>

static const uint32_t MAX_FRAME_DRAWS = 2;
static const uint32_t MAX_OBJECTS = 2;

static const std::vector<const char*> s_validationLayers =
{
	"VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> s_deviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
static const bool s_enableValidationLayers = false;
#else
static const bool s_enableValidationLayers = true;
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData)
{
	if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		std::cerr << "validation layer: " << callbackData->pMessage << std::endl;
	}

	return VK_FALSE;
}

static VkResult createDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void destroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
	createInfo.pUserData = nullptr;
}

VulkanRenderer::VulkanRenderer()
{
}

VulkanRenderer::~VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* window)
{
	m_window = window;

	try
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapchain();
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		createSynchronisation();
		allocateDynamicBufferTransferSpace();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();

		m_uboViewProjection.projection = glm::perspective(glm::radians(45.0f), 
			(float)(m_swapchainImageExtent.width / m_swapchainImageExtent.height), 
			0.1f, 100.0f);
		m_uboViewProjection.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		m_uboViewProjection.projection[1][1] *= -1;

		std::vector<Vertex> meshVertices =
		{
			{{-0.4f,  0.4f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // 0
			{{-0.4f, -0.4f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // 1
			{{ 0.4f, -0.4f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // 2
			{{ 0.4f,  0.4f, 0.0f}, {1.0f, 1.0f, 0.0f}}, // 3
		};
		std::vector<Vertex> meshVertices2 =
		{
			{{-0.25f,  0.6f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // 0
			{{-0.25f, -0.6f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // 1
			{{ 0.25f, -0.6f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // 2
			{{ 0.25f,  0.6f, 0.0f}, {1.0f, 1.0f, 0.0f}}, // 3
		};
		std::vector<uint32_t> indices =
		{
			0, 1, 2,
			2, 3, 0
		};
		m_meshes.emplace_back(Mesh(m_mainDevice.physicalDevice, m_mainDevice.logicalDevice, 
			m_graphicsQueue, m_graphicsCommandPool, 
			&meshVertices, &indices));
		m_meshes.emplace_back(Mesh(m_mainDevice.physicalDevice, m_mainDevice.logicalDevice,
			m_graphicsQueue, m_graphicsCommandPool,
			&meshVertices2, &indices));

		glm::mat4 firstMeshModelMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		m_meshes[0].setModel(firstMeshModelMatrix);

		recordCommands();
	}
	catch (const std::exception& e)
	{
		printf("%s\n", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}

void VulkanRenderer::destroy()
{
	vkDeviceWaitIdle(m_mainDevice.logicalDevice);

	_aligned_free(m_modelTransferSpace);

	vkDestroyDescriptorPool(m_mainDevice.logicalDevice, m_descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_mainDevice.logicalDevice, m_descriptorSetLayout, nullptr);

	for (size_t i = 0; i < m_swapchainImages.size(); i++)
	{
		vkDestroyBuffer(m_mainDevice.logicalDevice, m_vpUniformBuffers[i], nullptr);
		vkFreeMemory(m_mainDevice.logicalDevice, m_vpUniformBuffersMemory[i], nullptr);

		vkDestroyBuffer(m_mainDevice.logicalDevice, m_modelDUniformBuffers[i], nullptr);
		vkFreeMemory(m_mainDevice.logicalDevice, m_modelDUniformBuffersMemory[i], nullptr);
	}

	for (auto& mesh : m_meshes)
	{
		mesh.destroy();
	}

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		vkDestroySemaphore(m_mainDevice.logicalDevice, m_renderFinished[i], nullptr);
		vkDestroySemaphore(m_mainDevice.logicalDevice, m_imageAvailable[i], nullptr);
		vkDestroyFence(m_mainDevice.logicalDevice, m_drawFences[i], nullptr);
	}
	vkDestroyCommandPool(m_mainDevice.logicalDevice, m_graphicsCommandPool, nullptr);
	for (auto framebuffer : m_swapchainFramebuffers)
	{
		vkDestroyFramebuffer(m_mainDevice.logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(m_mainDevice.logicalDevice, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_mainDevice.logicalDevice, m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_mainDevice.logicalDevice, m_renderPass, nullptr);
	for (auto& image : m_swapchainImages)
	{
		vkDestroyImageView(m_mainDevice.logicalDevice, image.imageView, nullptr);
	}
	vkDestroySwapchainKHR(m_mainDevice.logicalDevice, m_swapchain, nullptr);
	vkDestroyDevice(m_mainDevice.logicalDevice, nullptr);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	if (s_enableValidationLayers)
	{
		destroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
	}
	vkDestroyInstance(m_instance, nullptr);
}

void VulkanRenderer::draw()
{
	vkWaitForFences(m_mainDevice.logicalDevice, 1, &m_drawFences[currentFrame],
		VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_mainDevice.logicalDevice, 1, &m_drawFences[currentFrame]);

	uint32_t imageIndex = 0;
	vkAcquireNextImageKHR(m_mainDevice.logicalDevice, m_swapchain,
		std::numeric_limits<uint64_t>::max(), m_imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

	updateUniformBuffers(imageIndex);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_imageAvailable[currentFrame];
	VkPipelineStageFlags waitStages[] =
	{
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_renderFinished[currentFrame];

	VkResult result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_drawFences[currentFrame]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit command buffer to queue!");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderFinished[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(m_presentationQueue, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present to a queue");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::updateModel(uint32_t modelId, glm::mat4 model)
{
	if (modelId >= m_meshes.size())
		return;

	m_meshes[modelId].setModel(model);
}

void VulkanRenderer::createInstance()
{
	if (s_enableValidationLayers && !checkValidationLayerSupport())
	{
		throw std::runtime_error("Validation layers requested, but not available!");
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Learn Vulkan";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions();

	if (!checkInstanceExtensionSupport(extensions))
	{
		throw std::runtime_error("GLFW required instance extensions are not supported!");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (s_enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(s_validationLayers.size());
		createInfo.ppEnabledLayerNames = s_validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an instance!");
	}
}

void VulkanRenderer::setupDebugMessenger()
{
	if (!s_enableValidationLayers)
	{
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	populateDebugMessengerCreateInfo(createInfo);

	VkResult result = createDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to setup debug messenger!");
	}
}

void VulkanRenderer::createSurface()
{
	VkResult result = glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create surface!");
	}
}

void VulkanRenderer::createLogicalDevice()
{
	QueueFamilyIndices indices = getQueueFamilies(m_mainDevice.physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
	std::set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };

	for (const int& queueFamilyIndex : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
		deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		deviceQueueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		deviceQueueCreateInfo.queueCount = 1;
		float priority = 1.0f;
		deviceQueueCreateInfo.pQueuePriorities = &priority;

		deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
	}

	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(s_deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = s_deviceExtensions.data();

	VkPhysicalDeviceFeatures physicalDeviceFeatures{};
	deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;

	VkResult result = vkCreateDevice(m_mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &m_mainDevice.logicalDevice);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create logical device!");
	}

	vkGetDeviceQueue(m_mainDevice.logicalDevice, indices.graphicsFamily, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_mainDevice.logicalDevice, indices.presentationFamily, 0, &m_presentationQueue);
}

void VulkanRenderer::createSwapchain()
{
	SwapchainDetails swapchainDetails = getSwapchainDetails(m_mainDevice.physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapchainDetails.formats);
	VkPresentModeKHR presentMode = chooseBestPresentMode(swapchainDetails.presentationModes);
	VkExtent2D surfaceExtent = chooseSurfaceExtent(swapchainDetails.surfaceCapabilities);

	uint32_t minImageCount = swapchainDetails.surfaceCapabilities.minImageCount + 1; // For enabling the triple buffer
	if (minImageCount > swapchainDetails.surfaceCapabilities.maxImageCount)
	{
		minImageCount = swapchainDetails.surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo{};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = m_surface;
	swapchainCreateInfo.minImageCount = minImageCount;
	swapchainCreateInfo.imageFormat = surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = surfaceExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = getQueueFamilies(m_mainDevice.physicalDevice);
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		uint32_t queueFamilyIndices[] = {
			(uint32_t)indices.graphicsFamily,
			(uint32_t)indices.presentationFamily
		};

		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount = 2;
		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	swapchainCreateInfo.preTransform = swapchainDetails.surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = presentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	VkResult result = vkCreateSwapchainKHR(m_mainDevice.logicalDevice, &swapchainCreateInfo, nullptr, &m_swapchain);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swapchain!");
	}

	m_swapchainImageFormat = surfaceFormat.format;
	m_swapchainImageExtent = surfaceExtent;

	uint32_t swapchainImageCount = 0;
	vkGetSwapchainImagesKHR(m_mainDevice.logicalDevice, m_swapchain, &swapchainImageCount, nullptr);

	std::vector<VkImage> swapchainImages(swapchainImageCount);
	vkGetSwapchainImagesKHR(m_mainDevice.logicalDevice, m_swapchain, &swapchainImageCount, swapchainImages.data());

	for (VkImage image : swapchainImages)
	{
		SwapchainImage swapchainImage{};
		swapchainImage.image = image;
		swapchainImage.imageView = createImageView(image, m_swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		m_swapchainImages.push_back(swapchainImage);
	}
}

void VulkanRenderer::createRenderPass()
{
	// -- RENDER PASS ATTACHMENTS --
	// Color attachment description
	VkAttachmentDescription colorAttachmentDescription{};
	colorAttachmentDescription.format = m_swapchainImageFormat;
	colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// -- SUBPASS --
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription{};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorAttachmentRef;

	// -- SUBPASS DEPENDENCY --
	std::array<VkSubpassDependency, 2> subpassDepenencies;
	// Conversion from VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	// Transition must happen after...
	subpassDepenencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDepenencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDepenencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	// Transition must happen before...
	subpassDepenencies[0].dstSubpass = 0;
	subpassDepenencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDepenencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	subpassDepenencies[0].dependencyFlags = 0;

	// Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	// Transition must happen after...
	subpassDepenencies[1].srcSubpass = 0;
	subpassDepenencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDepenencies[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	// Transition must happen before...
	subpassDepenencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDepenencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDepenencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDepenencies[1].dependencyFlags = 0;

	// -- RENDER PASS --
	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachmentDescription;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDescription;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDepenencies.size());
	renderPassCreateInfo.pDependencies = subpassDepenencies.data();

	VkResult result = vkCreateRenderPass(m_mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &m_renderPass);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create the render pass!");
	}
}

void VulkanRenderer::createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding vpBindingLayout{};
	vpBindingLayout.binding = 0;
	vpBindingLayout.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpBindingLayout.descriptorCount = 1;
	vpBindingLayout.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vpBindingLayout.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding modelBindingLayout{};
	modelBindingLayout.binding = 1;
	modelBindingLayout.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelBindingLayout.descriptorCount = 1;
	modelBindingLayout.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	modelBindingLayout.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings = { vpBindingLayout, modelBindingLayout };

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	layoutCreateInfo.pBindings = layoutBindings.data();

	VkResult result = vkCreateDescriptorSetLayout(m_mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &m_descriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Descriptor Set Layout!");
	}
}

void VulkanRenderer::createGraphicsPipeline()
{
	auto vertexShaderCode = readFile("resources/shaders/vert.spv");
	auto fragmentShaderCode = readFile("resources/shaders/frag.spv");

	// -- SHADER MODULE --
	VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo{};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	// -- VERTEX INPUT --
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::array<VkVertexInputAttributeDescription, 2> attribDescriptions;
	attribDescriptions[0].location = 0;
	attribDescriptions[0].binding = 0;
	attribDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDescriptions[0].offset = offsetof(Vertex, pos);

	attribDescriptions[1].location = 1;
	attribDescriptions[1].binding = 0;
	attribDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDescriptions[1].offset = offsetof(Vertex, color);

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribDescriptions.size());
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = attribDescriptions.data();

	// -- VERTEX ASSEMBLY --
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	// -- VIEWPORT & SCISSORS --
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)m_swapchainImageExtent.width;
	viewport.height = (float)m_swapchainImageExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissors{};
	scissors.offset = { 0, 0 };
	scissors.extent = m_swapchainImageExtent;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissors;

	// -- DYANMIC STATES --
	/*
	std::vector<VkDynamicState> dymanicStatesEnabled;
	dymanicStatesEnabled.push_back(VK_DYNAMIC_STATE_VIEWPORT); // Dynamic viewport: Can be resize using vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
	dymanicStatesEnabled.push_back(VK_DYNAMIC_STATE_SCISSOR);  // Dynamic scissors: Can be resize using vkCmdSetScissors(commandbuffer, 0, 1, &scissors);

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dymanicStatesEnabled.size());
	dynamicStateCreateInfo.pDynamicStates = dymanicStatesEnabled.data();
	*/

	// -- RASTERIZATION STATE --
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo{};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizerCreateInfo.lineWidth = 1.0f;

	// -- MULTISAMPING --
	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo{};
	multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE;

	// -- BLENDING --
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	// Formula: (srcColorBlendFactor * newColor) blend op (dstColorBlendFactor * oldColor)
	//			(newColor.a * newColor) + ((1 - newColor.a) * oldColor)
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	// Formula: (srcAlphaBlendFactor * newAlpha) blend op (dstAlphaBlendFactor * oldAlpha)
	//			(1 * newAlpha) + (0 * oldAlpha) = newAlpha
	colorBlendAttachmentState.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	// -- PIPELINE LAYOUT --
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VkResult result = vkCreatePipelineLayout(m_mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout!");
	}

	// -- DEPTH STENCIL TESTING --
	// TODO: Set up depth stencil testing

	// -- GRAPHICS PIPELINE --
	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStages;
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = nullptr;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = nullptr;
	graphicsPipelineCreateInfo.layout = m_pipelineLayout;	// Pipeline layout description that should be used
	graphicsPipelineCreateInfo.renderPass = m_renderPass;	// A render pass that will be compatible with graphics pipeline
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(m_mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_graphicsPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(m_mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(m_mainDevice.logicalDevice, vertexShaderModule, nullptr);
}

void VulkanRenderer::createFramebuffers()
{
	m_swapchainFramebuffers.resize(m_swapchainImages.size());

	for (size_t i = 0; i < m_swapchainFramebuffers.size(); i++)
	{
		std::array<VkImageView, 1> attachments =
		{
			m_swapchainImages[i].imageView
		};

		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = m_renderPass;
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();
		framebufferCreateInfo.width = m_swapchainImageExtent.width;
		framebufferCreateInfo.height = m_swapchainImageExtent.height;
		framebufferCreateInfo.layers = 1;

		VkResult result = vkCreateFramebuffer(m_mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &m_swapchainFramebuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a framebuffer!");
		}
	}
}

void VulkanRenderer::createCommandPool()
{
	QueueFamilyIndices indices = getQueueFamilies(m_mainDevice.physicalDevice);

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo{};
	graphicsCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	graphicsCommandPoolCreateInfo.queueFamilyIndex = indices.graphicsFamily;

	VkResult result = vkCreateCommandPool(m_mainDevice.logicalDevice, &graphicsCommandPoolCreateInfo, nullptr, &m_graphicsCommandPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command pool!");
	}
}

void VulkanRenderer::createCommandBuffers()
{
	m_commandBuffers.resize(m_swapchainFramebuffers.size());

	VkCommandBufferAllocateInfo commandBuffersAllocInfo{};
	commandBuffersAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBuffersAllocInfo.commandPool = m_graphicsCommandPool;
	commandBuffersAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	// VK_COMMAND_BUFFER_LEVEL_PRIMARY	 : Supposed to be used directly by the queue.
	// VK_COMMAND_BUFFER_LEVEL_SECONDARY : Only to be used(called) by or from other primary level command buffer
	commandBuffersAllocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

	VkResult result = vkAllocateCommandBuffers(m_mainDevice.logicalDevice, &commandBuffersAllocInfo, m_commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command buffers!");
	}
}

void VulkanRenderer::createSynchronisation()
{
	m_imageAvailable.resize(MAX_FRAME_DRAWS);
	m_renderFinished.resize(MAX_FRAME_DRAWS);
	m_drawFences.resize(MAX_FRAME_DRAWS);

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		if (vkCreateSemaphore(m_mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &m_imageAvailable[i]) != VK_SUCCESS ||
			vkCreateSemaphore(m_mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &m_renderFinished[i]) != VK_SUCCESS ||
			vkCreateFence(m_mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &m_drawFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create semaphore and/or fence!");
		}
	}
}

void VulkanRenderer::createUniformBuffers()
{
	m_vpUniformBuffers.resize(m_swapchainImages.size());
	m_vpUniformBuffersMemory.resize(m_swapchainImages.size());
	m_modelDUniformBuffers.resize(m_swapchainImages.size());
	m_modelDUniformBuffersMemory.resize(m_swapchainImages.size());

	VkDeviceSize vpBufferSize = sizeof(UboViewProjection);
	VkDeviceSize modelBufferSize = m_modelUniformAlignment * MAX_OBJECTS;

	for (size_t i = 0; i < m_swapchainImages.size(); i++)
	{
		BufferUtilities::createBuffer(m_mainDevice.physicalDevice, m_mainDevice.logicalDevice, vpBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&m_vpUniformBuffers[i], &m_vpUniformBuffersMemory[i]);

		BufferUtilities::createBuffer(m_mainDevice.physicalDevice, m_mainDevice.logicalDevice, modelBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&m_modelDUniformBuffers[i], &m_modelDUniformBuffersMemory[i]);
	}
}

void VulkanRenderer::createDescriptorPool()
{
	VkDescriptorPoolSize vpPoolSize{};
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(m_vpUniformBuffers.size());

	VkDescriptorPoolSize modelPoolSize{};
	modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelPoolSize.descriptorCount = static_cast<uint32_t>(m_modelDUniformBuffers.size());

	std::vector<VkDescriptorPoolSize> descriptorPoolSize = { vpPoolSize, modelPoolSize };

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = static_cast<uint32_t>(m_swapchainImages.size());
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSize.size());
	poolCreateInfo.pPoolSizes = descriptorPoolSize.data();

	VkResult result = vkCreateDescriptorPool(m_mainDevice.logicalDevice, &poolCreateInfo, nullptr, &m_descriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool!");
	}
}

void VulkanRenderer::createDescriptorSets()
{
	m_descriptorSets.resize(m_swapchainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(m_swapchainImages.size(), m_descriptorSetLayout);

	VkDescriptorSetAllocateInfo setAllocInfo{};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = m_descriptorPool;
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(m_swapchainImages.size());
	setAllocInfo.pSetLayouts = setLayouts.data();

	VkResult result = vkAllocateDescriptorSets(m_mainDevice.logicalDevice, &setAllocInfo, m_descriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor pool!");
	}

	for (size_t i = 0; i < m_swapchainImages.size(); i++)
	{
		VkDescriptorBufferInfo vpBufferInfo{};
		vpBufferInfo.buffer = m_vpUniformBuffers[i];
		vpBufferInfo.offset = 0;
		vpBufferInfo.range = sizeof(UboViewProjection);

		VkWriteDescriptorSet vpWriteDescriptorSet{};
		vpWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vpWriteDescriptorSet.dstSet = m_descriptorSets[i];
		vpWriteDescriptorSet.dstBinding = 0;
		vpWriteDescriptorSet.dstArrayElement = 0;
		vpWriteDescriptorSet.descriptorCount = 1;
		vpWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpWriteDescriptorSet.pBufferInfo = &vpBufferInfo;

		VkDescriptorBufferInfo modelBufferInfo{};
		modelBufferInfo.buffer = m_modelDUniformBuffers[i];
		modelBufferInfo.offset = 0;
		modelBufferInfo.range = m_modelUniformAlignment;

		VkWriteDescriptorSet modelWriteDescriptorSet{};
		modelWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		modelWriteDescriptorSet.dstSet = m_descriptorSets[i];
		modelWriteDescriptorSet.dstBinding = 1;
		modelWriteDescriptorSet.dstArrayElement = 0;
		modelWriteDescriptorSet.descriptorCount = 1;
		modelWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		modelWriteDescriptorSet.pBufferInfo = &modelBufferInfo;

		std::vector<VkWriteDescriptorSet> descriptorSetWrites = { vpWriteDescriptorSet, modelWriteDescriptorSet };

		vkUpdateDescriptorSets(m_mainDevice.logicalDevice,
			static_cast<uint32_t>(descriptorSetWrites.size()),
			descriptorSetWrites.data(),
			0, nullptr);
	}
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
	void* data = nullptr;
	vkMapMemory(m_mainDevice.logicalDevice, m_vpUniformBuffersMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
	memcpy(data, &m_uboViewProjection, sizeof(UboViewProjection));
	vkUnmapMemory(m_mainDevice.logicalDevice, m_vpUniformBuffersMemory[imageIndex]);

	for (uint32_t i = 0; i < m_meshes.size(); i++)
	{
		UboModel* thisModel = (UboModel*)((uint64_t)m_modelTransferSpace + (i * m_modelUniformAlignment));
		*thisModel = m_meshes[i].getModel();
	}

	vkMapMemory(m_mainDevice.logicalDevice, m_modelDUniformBuffersMemory[imageIndex], 0, m_modelUniformAlignment * m_meshes.size(), 0, &data);
	memcpy(data, m_modelTransferSpace, m_modelUniformAlignment * m_meshes.size());
	vkUnmapMemory(m_mainDevice.logicalDevice, m_modelDUniformBuffersMemory[imageIndex]);
}

void VulkanRenderer::recordCommands()
{
	VkCommandBufferBeginInfo commandBufferBeginInfo{};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_renderPass;
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = m_swapchainImageExtent;
	renderPassBeginInfo.clearValueCount = 1;
	VkClearValue clearValues[] = {
		{0.6f, 0.65f, 0.4f, 1.0f}
	};
	renderPassBeginInfo.pClearValues = clearValues;

	for (size_t i = 0; i < m_commandBuffers.size(); i++)
	{
		renderPassBeginInfo.framebuffer = m_swapchainFramebuffers[i];

		VkResult result = vkBeginCommandBuffer(m_commandBuffers[i], &commandBufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to begin the command buffer!");
		}

		vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

		for (uint32_t j = 0; j < m_meshes.size(); j++)
		{
			auto& mesh = m_meshes[j];

			VkBuffer vertexBuffers[] = { mesh.getVertexBuffer() };
			VkDeviceSize offsets[] = { 0 };

			vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(m_commandBuffers[i], mesh.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

			uint32_t dynamicOffset = static_cast<uint32_t>(m_modelUniformAlignment) * j;

			vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
				&m_descriptorSets[i], 1, &dynamicOffset);

			//vkCmdDraw(m_commandBuffers[i], m_mesh.getVertexCount(), 1, 0, 0);
			vkCmdDrawIndexed(m_commandBuffers[i], mesh.getIndexCount(), 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(m_commandBuffers[i]);

		result = vkEndCommandBuffer(m_commandBuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to end the command buffer!");
		}
	}
}

void VulkanRenderer::getPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("No GPUs found that supports Vulkan!");
	}

	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, deviceList.data());

	for (const auto& device : deviceList)
	{
		if (checkDeviceSuitable(device))
		{
			m_mainDevice.physicalDevice = device;
			break;
		}
	}

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(m_mainDevice.physicalDevice, &deviceProperties);

	m_minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

void VulkanRenderer::allocateDynamicBufferTransferSpace()
{
	m_modelUniformAlignment = (sizeof(UboModel) + m_minUniformBufferOffset - 1) & ~(m_minUniformBufferOffset - 1);

	m_modelTransferSpace = (UboModel*)_aligned_malloc(m_modelUniformAlignment * MAX_OBJECTS, m_modelUniformAlignment);
}

bool VulkanRenderer::checkInstanceExtensionSupport(const std::vector<const char*>& checkExtensions)
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	for (const char* checkExtension : checkExtensions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (std::strcmp(checkExtension, extension.extensionName))
			{
				hasExtension = true;
				break;
			}
		}

		if (!hasExtension)
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	QueueFamilyIndices indices = getQueueFamilies(device);

	bool supportExtensions = checkDeviceExtensionSupport(device);

	SwapchainDetails swapchainDetails = getSwapchainDetails(device);
	bool isValidSwapchain = !swapchainDetails.formats.empty() &&
		!swapchainDetails.presentationModes.empty();

	return indices.isValid() && supportExtensions && isValidSwapchain;
}

bool VulkanRenderer::checkValidationLayerSupport()
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* validationLayer : s_validationLayers)
	{
		bool layerFound = false;

		for (const auto& availableLayer : availableLayers)
		{
			if (std::strcmp(validationLayer, availableLayer.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	for (const char* deviceExtension : s_deviceExtensions)
	{
		bool hasExtension = false;

		for (const auto& extension : extensions)
		{
			if (std::strcmp(deviceExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
			}
		}

		if (!hasExtension)
		{
			return false;
		}
	}

	return true;
}

std::vector<const char*> VulkanRenderer::getRequiredExtensions() const
{
	uint32_t glfwExtenionCount = 0;
	const char** glfwExtensions = nullptr;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtenionCount);

	std::vector<const char*> extension(glfwExtensions, glfwExtensions + glfwExtenionCount);

	if (s_enableValidationLayers)
	{
		extension.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extension;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device)
{
	/*
	// VkPhysicalDeviceProperties devicePropertices;
	// vkGetPhysicalDeviceProperties(device, &devicePropertices);

	// VkPhysicalDeviceFeatures deviceFeatures;
	// vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	*/

	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilyList)
	{
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentationSupport);
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		if (indices.isValid())
		{
			break;
		}

		i++;
	}

	return indices;
}

SwapchainDetails VulkanRenderer::getSwapchainDetails(VkPhysicalDevice device)
{
	SwapchainDetails swapchainDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &swapchainDetails.surfaceCapabilities);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

	if (formatCount > 0)
	{
		swapchainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, swapchainDetails.formats.data());
	}

	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationCount, nullptr);

	if (presentationCount > 0)
	{
		swapchainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationCount, swapchainDetails.presentationModes.data());
	}

	return swapchainDetails;
}

VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& surfaceFormat : formats)
	{
		if ((surfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM ||
			surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM) &&
			surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return surfaceFormat;
		}
	}

	return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentMode(const std::vector<VkPresentModeKHR>& presentationModes)
{
	for (const auto& presentMode : presentationModes)
	{
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSurfaceExtent(VkSurfaceCapabilitiesKHR surfaceCapabilities)
{
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}

	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	VkExtent2D newExtent{};
	newExtent.width = static_cast<uint32_t>(width);
	newExtent.height = static_cast<uint32_t>(height);

	newExtent.width = std::clamp(newExtent.width,
		surfaceCapabilities.minImageExtent.width,
		surfaceCapabilities.maxImageExtent.width);
	newExtent.height = std::clamp(newExtent.height,
		surfaceCapabilities.minImageExtent.height,
		surfaceCapabilities.maxImageExtent.height);

	return newExtent;
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewCreateInfo{};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	VkResult result = vkCreateImageView(m_mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an Image View!");
	}

	return imageView;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& shaderCode)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo{};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = shaderCode.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(m_mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module!");
	}

	return shaderModule;
}
