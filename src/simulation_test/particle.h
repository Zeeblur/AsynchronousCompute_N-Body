#pragma once
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>

// structs to store vertex attributes
struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 texCoord;

	Vertex(glm::vec3 p, glm::vec3 n, glm::vec2 t);

	// how to pass to vertex shader
	static VkVertexInputBindingDescription getBindingDescription();

	// get attribute descriptions...
	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescription();
};

struct particle
{
	glm::vec4 pos;								// Particle position
	glm::vec4 vel;								// Particle velocity
};

 