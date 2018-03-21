#include "particle.h"

// structs to store vertex attributes
Vertex::Vertex(glm::vec3 p, glm::vec3 c, glm::vec2 t) : pos(p), colour(c), texCoord(t) {}

// how to pass to vertex shader
VkVertexInputBindingDescription Vertex::getBindingDescription()
{
	VkVertexInputBindingDescription bindingDescription = {};

	// 1 binding as all data is in 1 array. binding is index.
	// stride is bytes between entries (in this case 1 whole vertex struct)
	// move to the next data entry after each vertex (not instanced rendering)
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;


	return bindingDescription;
}

// get attribute descriptions...
std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescription()
{
	// 2 attributes (position and colour) so two description structs
	std::array<VkVertexInputAttributeDescription, 3> attributeDesc = {};

	attributeDesc[0].binding = 0; // which binding (the only one created above)
	attributeDesc[0].location = 0; // which location of the vertex shader
	attributeDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT; // format as a vector 3 (3floats)
	attributeDesc[0].offset = offsetof(Vertex, pos); // calculate the offset within each Vertex

													 // as above but for colour
	attributeDesc[1].binding = 0;
	attributeDesc[1].location = 1;
	attributeDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT; // format as a vector 3 (3floats)
	attributeDesc[1].offset = offsetof(Vertex, colour);

	// texture layout
	attributeDesc[2].binding = 0;
	attributeDesc[2].location = 2;
	attributeDesc[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDesc[2].offset = offsetof(Vertex, texCoord);

	return attributeDesc;
}