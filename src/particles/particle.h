#pragma once

//struct UboInstanceData
//{
//	// Model matrix
//	glm::mat4 model;
//};
//
//struct
//{
//	// Global matrices
//	struct
//	{
//		glm::mat4 projection;
//		glm::mat4 view;
//	} matrices;
//	// Seperate data for each instance
//	UboInstanceData *instance;
//} uboParticlessa;

struct particle
{
	glm::vec4 pos;								// Particle position
	glm::vec4 vel;								// Particle velocity
};

 