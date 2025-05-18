#include "mesh.h"

Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device, std::vector<Vertex>* vertices)
{
	m_physicalDevice = physicalDevice;
	m_device = device;
	m_vertexCount = vertices->size();
	createVertexBuffer(vertices);
}

void Mesh::destroy()
{
	vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
	vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
}

void Mesh::createVertexBuffer(std::vector<Vertex>* vertices)
{
	VkBufferCreateInfo vertexBufferCreateInfo{};
	vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferCreateInfo.size = sizeof(Vertex) * vertices->size();
	vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	vertexBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(m_device, &vertexBufferCreateInfo, nullptr, &m_vertexBuffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vertex buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_device, m_vertexBuffer, &memRequirements);

	VkMemoryAllocateInfo memoryAllocInfo{};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	result = vkAllocateMemory(m_device, &memoryAllocInfo, nullptr, &m_vertexBufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate device memory!");
	}

	vkBindBufferMemory(m_device, m_vertexBuffer, m_vertexBufferMemory, 0);

	void* data = nullptr;
	vkMapMemory(m_device, m_vertexBufferMemory, 0, vertexBufferCreateInfo.size, 0, &data);
	memcpy(data, vertices->data(), (size_t)vertexBufferCreateInfo.size);
	vkUnmapMemory(m_device, m_vertexBufferMemory);
}

uint32_t Mesh::findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags memPropertyFlags) const
{
	VkPhysicalDeviceMemoryProperties memoryPropertices;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryPropertices);

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
