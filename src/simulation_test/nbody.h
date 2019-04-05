#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <memory>
#include "Renderer.h"
#include "particle.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <map>

enum MODE
{
	COMPUTE,
	TRANSFER,
	DOUBLE
};

extern struct parameters
{
	// default values for simulation
	uint32_t totalTime = 120;
	uint32_t pCount = 2000;
	uint32_t stacks = 20;
	uint32_t slices = 20;
	glm::vec3 dims = glm::vec3(0.02f);
	bool lighting = false;
	MODE chosenMode;

	char *modeTypes[3] =
	{
		"NORMAL COMPUTE",
		"TRANSFER BUFFERS _ ASYNC",
		"DOUBLE BUFFERING _ ASYNC"
	};

	void print()
	{
		std::cout << "Simulation Mode: " << modeTypes[chosenMode] << std::endl;
		std::cout << "Time to Run Sim (seconds): " << totalTime << std::endl;
		std::cout << "Particle count: " << pCount << std::endl;
		std::cout << "Stacks: " << stacks << std::endl;
		std::cout << "Slices: " << slices << std::endl;
		std::cout << "Lighting: " << (lighting ? "On" : "Off") << std::endl;
	}
};

class nbody
{

	const int WIDTH = 800;
	const int HEIGHT = 600;
/*
	void initialiseVulkan(const bool AMD);
*/
	std::vector<particle> particleBuffer;
	std::vector<Vertex> vertexBuffer;
	std::vector<uint16_t> indexBuffer;

	void createSphereGeom(const unsigned int stacks, const unsigned int slices, const glm::vec3 dims);
	void computeTangentBasis(std::vector<Vertex*> &triangleVertices, std::map <Vertex*, std::vector<glm::vec3>> &tangents, std::map <Vertex*, std::vector<glm::vec3>> &bitangents);

public:

	nbody(const unsigned int num, const bool AMD, const MODE chosenMode);

	~nbody();
	
	void prepareParticles();

	void run(const parameters simParam); // default 2 mins


	unsigned int num_particles = 0;

};