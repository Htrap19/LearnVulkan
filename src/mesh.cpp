#include "mesh.h"

Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device,
	VkQueue transferQueue, VkCommandPool transferCommandPool,
	std::vector<Vertex>* vertices, std::vector<uint32_t>* indices)
{
	m_physicalDevice = physicalDevice;
	m_device = device;
	m_vertexCount = vertices->size();
	createVertexBuffer(transferQueue, transferCommandPool, vertices);
	m_indexCount = indices->size();
	createIndexBuffer(transferQueue, transferCommandPool, indices);

	m_model.model = glm::mat4(1.0f);
}

void Mesh::destroy()
{
	vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
	vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
	vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
	vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
}

void Mesh::createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, 
	std::vector<Vertex>* vertices)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingDeviceMemory;

	VkDeviceSize bufferSize = sizeof(Vertex) * vertices->size();
	BufferUtilities::createBuffer(m_physicalDevice, m_device,
		bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
		&stagingBuffer, &stagingDeviceMemory);

	void* data = nullptr;
	vkMapMemory(m_device, stagingDeviceMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices->data(), (size_t)bufferSize);
	vkUnmapMemory(m_device, stagingDeviceMemory);

	BufferUtilities::createBuffer(m_physicalDevice, m_device,
		bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		&m_vertexBuffer, &m_vertexBufferMemory);

	BufferUtilities::copyBuffer(m_device, transferQueue, transferCommandPool, 
		stagingBuffer, m_vertexBuffer, bufferSize);

	vkDestroyBuffer(m_device, stagingBuffer, nullptr);
	vkFreeMemory(m_device, stagingDeviceMemory, nullptr);
}

void Mesh::createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>* indices)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingDeviceMemory;

	VkDeviceSize bufferSize = sizeof(uint32_t) * indices->size();
	BufferUtilities::createBuffer(m_physicalDevice, m_device,
		bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, &stagingDeviceMemory);

	void* data = nullptr;
	vkMapMemory(m_device, stagingDeviceMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices->data(), (size_t)bufferSize);
	vkUnmapMemory(m_device, stagingDeviceMemory);

	BufferUtilities::createBuffer(m_physicalDevice, m_device,
		bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&m_indexBuffer, &m_indexBufferMemory);

	BufferUtilities::copyBuffer(m_device, transferQueue, transferCommandPool,
		stagingBuffer, m_indexBuffer, bufferSize);

	vkDestroyBuffer(m_device, stagingBuffer, nullptr);
	vkFreeMemory(m_device, stagingDeviceMemory, nullptr);
}

void BufferUtilities::createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, 
	VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, 
	VkMemoryPropertyFlags memoryPropertices, 
	VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
	VkBufferCreateInfo vertexBufferCreateInfo{};
	vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferCreateInfo.size = bufferSize;
	vertexBufferCreateInfo.usage = bufferUsage;
	vertexBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(device, &vertexBufferCreateInfo, nullptr, buffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

	VkMemoryAllocateInfo memoryAllocInfo{};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, 
		memRequirements.memoryTypeBits, memoryPropertices);

	result = vkAllocateMemory(device, &memoryAllocInfo, nullptr, bufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate device memory!");
	}

	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

void BufferUtilities::copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	VkCommandBuffer transferCommandBuffer;

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = transferCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkResult result = vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate transfer command buffer while copying buffer!");
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

	VkBufferCopy copyBufferRegion{};
	copyBufferRegion.srcOffset = 0;
	copyBufferRegion.dstOffset = 0;
	copyBufferRegion.size = bufferSize;

	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &copyBufferRegion);

	vkEndCommandBuffer(transferCommandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommandBuffer;

	vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(transferQueue);

	vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}

uint32_t BufferUtilities::findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags memPropertyFlags)
{
	VkPhysicalDeviceMemoryProperties memoryPropertices;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryPropertices);

	for (uint32_t i = 0; i < memoryPropertices.memoryTypeCount; i++)
	{
		if ((allowedTypes & (1 << i)) &&
			(memoryPropertices.memoryTypes[i].propertyFlags & memPropertyFlags) == memPropertyFlags)
		{
			return i;
		}
	}

	throw std::runtime_error("Unable to find suitable memory type index!");
	return 0;
}
