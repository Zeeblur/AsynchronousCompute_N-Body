#include "particle.h"

particle::particle()
{

}

// create vertex buffer objs
void particle::createVertexBuffer()
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	// create a staging buffer as a temp buffer and then the device has a local vertex  buffer.
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	Application::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

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