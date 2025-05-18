#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "utilities.h"

class Mesh
{
public:
	Mesh() = default;
	Mesh(VkPhysicalDevice physicalDevice, VkDevice device, std::vector<Vertex>* vertices);

	void destroy();

	inline int getVertexCount() const { return m_vertexCount; }
	inline VkBuffer getVertexBuffer() const { return m_vertexBuffer; }

private:
	void createVertexBuffer(std::vector<Vertex>* vertices);
	uint32_t findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags memProperties) const;

private:
	int m_vertexCount = 0;
	VkBuffer m_vertexBuffer;
	VkDeviceMemory m_vertexBufferMemory;

	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;
};

