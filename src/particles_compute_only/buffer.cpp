#include "buffer.h"
#include "nbody.h"
#include "renderer.h"
#include <array>

BufferObject::~BufferObject()
{
	vkDestroyBuffer(*dev, buffer, nullptr);
	vkFreeMemory(*dev, memory, nullptr);
}

void VertexBO::createSpecificBuffer()
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
	size = (size_t)bufferSize;

	// create a staging buffer as a temp buffer and then the *dev has a local vertex  buffer.
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	Renderer::get()->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	// copy data to buffer
	void* data;
	// map buffer memory to the cpu
	vkMapMemory(*dev, stagingBufferMemory, 0, bufferSize, 0, &data);

	// Copy vertex data into the mapped memory
	memcpy(data, vertices.data(), size);

	// unmap buffer
	vkUnmapMemory(*dev, stagingBufferMemory);

	// create vertex buffer
	Renderer::get()->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);

	// local so can't use map., so have to copy data between buffers.
	Renderer::get()->copyBuffer(stagingBuffer, buffer, bufferSize);

	// clean up staging buffer
	vkDestroyBuffer(*dev, stagingBuffer, nullptr);
	vkFreeMemory(*dev, stagingBufferMemory, nullptr);
}

void IndexBO::createSpecificBuffer()
{

	// buffersize is the number of incides times the size of the index type (unit32/16)
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
	size = indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	Renderer::get()->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(*dev, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(*dev, stagingBufferMemory);

	// note usage is INDEX buffer. 
	Renderer::get()->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);

	Renderer::get()->copyBuffer(stagingBuffer, buffer, bufferSize);

	vkDestroyBuffer(*dev, stagingBuffer, nullptr);
	vkFreeMemory(*dev, stagingBufferMemory, nullptr);
}

void InstanceBO::createSpecificBuffer()
{

	// buffersize is the number of incides times the size of the index type (unit32/16)
	VkDeviceSize bufferSize = sizeof(particles[0]) * particles.size();
	size = particles.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	Renderer::get()->createBuffer(bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory);

	void* data;
	vkMapMemory(*dev, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, particles.data(), (size_t)bufferSize);
	vkUnmapMemory(*dev, stagingBufferMemory);

	// note usage is INDEX buffer. and storage for compute
	Renderer::get()->createBuffer(bufferSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		//  for getting data back VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		buffer,
		memory);

	Renderer::get()->copyBuffer(stagingBuffer, buffer, bufferSize);

	vkDestroyBuffer(*dev, stagingBuffer, nullptr);
	vkFreeMemory(*dev, stagingBufferMemory, nullptr);
}

VkVertexInputBindingDescription InstanceBO::getBindingDescription()
{
	VkVertexInputBindingDescription vInputBindDescription{};
	vInputBindDescription.binding = 1;   // bind this to 1 (vertex is 0)
	vInputBindDescription.stride = sizeof(particle);
	vInputBindDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
	return vInputBindDescription;
}

std::array<VkVertexInputAttributeDescription, 2> InstanceBO::getAttributeDescription()
{
	// 1 attributes (position)
	std::array<VkVertexInputAttributeDescription, 2> attributeDesc;

	attributeDesc[0].binding = 1; // which binding (the only one created above)
	attributeDesc[0].location = 3; // which location of the vertex shader
	attributeDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT; // format as a vector 3 (3floats)
	attributeDesc[0].offset = offsetof(particle, pos); // calculate the offset within each Vertex


													   // Location 2 : Velocity
	attributeDesc[1].binding = 1; // which binding (the only one created above)
	attributeDesc[1].location = 4; // which location of the vertex shader
	attributeDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT; // format as a vector 3 (3floats)
	attributeDesc[1].offset = offsetof(particle, vel); // calculate the offset within each Vertex

	return attributeDesc;
}
