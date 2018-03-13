#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>
#include "particle.h"

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

enum bufferType
{
	VERTEX,
	INDEX,
	INSTANCE
};

struct BufferObject
{
	const VkDevice* dev;
	std::vector<VkBuffer> buffer;
	std::vector<VkDeviceMemory> memory;
	size_t size = 0;
	int bufferIndex = 0;

	virtual void createSpecificBuffer() = 0;

	~BufferObject();
};

struct VertexBO : BufferObject
{
	std::vector<Vertex> vertices;
	void createSpecificBuffer();
};

struct IndexBO : BufferObject
{
	std::vector<uint16_t> indices;

	void createSpecificBuffer();
};

struct InstanceBO : BufferObject
{
	std::vector<particle> particles;
	void createSpecificBuffer();
	void createDrawStorage();

	static VkVertexInputBindingDescription getBindingDescription();

	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescription();
};