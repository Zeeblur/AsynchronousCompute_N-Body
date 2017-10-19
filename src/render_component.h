#include <iostream>
#include "application.h"

struct particle
{
	VkDevice device = nullptr;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	uint32_t indexCount = 0;
	uint32_t vertexCount = 0;

	particle();
	~particle();
};