#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "utilities.h"

class BufferUtilities
{
public:
	static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device,
		VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
		VkMemoryPropertyFlags memoryPropertices,
		VkBuffer* buffer, VkDeviceMemory* bufferMemory);

	static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
		VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize);

	static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice,
		uint32_t allowedTypes, VkMemoryPropertyFlags memPropertyFlags);
};

class Mesh
{
public:
	Mesh() = default;
	Mesh(VkPhysicalDevice physicalDevice, VkDevice device, 
		VkQueue transferQueue, VkCommandPool transferCommandPool, 
		std::vector<Vertex>* vertices, std::vector<uint32_t>* indices);

	void destroy();

	inline int getVertexCount() const { return m_vertexCount; }
	inline VkBuffer getVertexBuffer() const { return m_vertexBuffer; }
	inline int getIndexCount() const { return m_indexCount; }
	inline VkBuffer getIndexBuffer() const { return m_indexBuffer; }

private:
	void createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, 
		std::vector<Vertex>* vertices);
	void createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,
		std::vector<uint32_t>* indices);

private:
	int m_vertexCount = 0;
	VkBuffer m_vertexBuffer;
	VkDeviceMemory m_vertexBufferMemory;

	int m_indexCount = 0;
	VkBuffer m_indexBuffer;
	VkDeviceMemory m_indexBufferMemory;

	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;
};

