#include "particle.h"

particle::particle()
{
	
}

std::vector<render_component*> _components;

// create vertex buffer objs
void render_system::createVertexBuffer()
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	// create a staging buffer as a temp buffer and then the device has a local vertex  buffer.
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	// copy data to buffer
	void* data;
	// map buffer memory to the cpu
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);

	// Copy vertex data into the mapped memory
	memcpy(data, vertices.data(), (size_t)bufferSize);

	// unmap buffer
	vkUnmapMemory(device, stagingBufferMemory);

	// create vertex buffer
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, saveBuffer, vertexBufferMemory);

	// local so can't use map., so have to copy data between buffers.
	copyBuffer(stagingBuffer, saveBuffer, bufferSize);

	// clean up staging buffer
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}


// create buffer in memory - allocate and map
void render_system::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	// create struct as usual
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	// how big is the buffer (enough to store the size of 1 obj * how many)
	bufferInfo.size = size;

	// what purpose is this buffer going to be used for ( can use multiple things)
	bufferInfo.usage = usage;

	// buffer only used by the graphics queue so it can be exclusive
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// create it
	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create buffer");
	}

	// Memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	// determine memory type to allocate 
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	// allocate memory
	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate buffer memory");

	// bind memory to the buffer (int is offset)
	vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

// copy data between buffers
void render_system::copyBuffer(VkBuffer srcBuff, VkBuffer targetBuff, VkDeviceSize size)
{
	// mem transfer exectued by command buffers (just like drawing commands)

	// allocate temporary command buffer
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	// allocate
	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	// immediately start recording commands
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // only going to use it once

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	// record copy commands
	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuff, targetBuff, 1, &copyRegion);


	// end commands
	vkEndCommandBuffer(commandBuffer);

	// execute the buffer to complete the transfer of data
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// wait for it to finish
	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	// cleanup
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

}

// create index buffer
void render_system::createIndexBuffer()
{
	// buffersize is the number of incides times the size of the index type (unit32/16)
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	// note usage is INDEX buffer. 
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

	copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}