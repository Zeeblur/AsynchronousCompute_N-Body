#include <iostream>
#include "application.h"

class render_system
{
	// pointer to device here
	std::shared_ptr<VkDevice> device;

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuff, VkBuffer targetBuff, VkDeviceSize size);
	void createVertexBuffer();//(VkBuffer &saveBuffer, std::vector<Vertex> &vertices);
	void createIndexBuffer();
};

struct render_component
{
	VkDevice device = nullptr;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	uint32_t indexCount = 0;
	uint32_t vertexCount = 0;
};

struct particle
{
	std::shared_ptr<render_component> renderdata; 

	particle();
	~particle();
};