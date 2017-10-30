#ifndef PARTICLE_H
#define PARTICLE_H

#include <glm/glm.hpp>

struct UboInstanceData
{
	// Model matrix
	glm::mat4 model;
};

struct
{
	// Global matrices
	struct
	{
		glm::mat4 projection;
		glm::mat4 view;
	} matrices;
	// Seperate data for each instance
	UboInstanceData *instance;
} uboParticles;

struct particle
{
	glm::vec3 pos;								// Particle position
	glm::vec3 vel;								// Particle velocity
};


#endif // !PARTICLE_H
