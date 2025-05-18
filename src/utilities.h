#pragma once

#include <fstream>

#include <glm/glm.hpp>

struct QueueFamilyIndices
{
	int graphicsFamily = -1;
	int presentationFamily = -1;

	bool isValid() const
	{
		return graphicsFamily >= 0 && 
			presentationFamily >= 0;
	}
};

struct SwapchainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentationModes;
};

struct SwapchainImage
{
	VkImage image;
	VkImageView imageView;
};

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
};

static std::vector<char> readFile(const std::string& filePath)
{
	std::ifstream file(filePath, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open a file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> fileBuffer(fileSize);

	file.seekg(0);

	file.read(fileBuffer.data(), fileSize);

	return fileBuffer;
}