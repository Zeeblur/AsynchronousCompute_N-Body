#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <memory>
#include "Renderer.h"
#include "particle.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

class nbody
{

	const int WIDTH = 800;
	const int HEIGHT = 600;

	void initialiseVulkan();

	std::vector<particle> particleBuffer;
	std::vector<Vertex> vertexBuffer;
	std::vector<uint16_t> indexBuffer;

	void createSphereGeom(const unsigned int stacks, const unsigned int slices, const glm::vec3 &dims);


public:

	// default num particles is 4
	nbody(const unsigned int num = 4);

	~nbody();
	
	void prepareParticles();

	void run();


	unsigned int num_particles = 0;

};