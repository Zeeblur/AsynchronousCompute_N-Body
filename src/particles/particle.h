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
	glm::vec3 pos;								// Particle position
	//glm::vec3 vel;								// Particle velocity
};

 