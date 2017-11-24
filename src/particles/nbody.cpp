#include "nbody.h"

using namespace glm;

// TODO: define vars for window/particles etc
nbody::nbody(const unsigned int num_particles)
{
	// initialise the application
	initialiseVulkan();
}

void nbody::initialiseVulkan()
{
	auto &app = Application::get();

	app->init();

}

void nbody::prepareParticles()
{
	particleBuffer = std::vector<particle>(num_particles);

	// create positions for particles
	for (size_t i = 0; i < num_particles; ++i)
	{
		//particle p;

		//p.pos = vec3::();

	}
}

void nbody::run()
{
	// loop here
	prepareParticles();
	createSphereGeom(20, 20, vec3(1.0));
	Application::get()->setVertexData(vertexBuffer, indexBuffer);

	Application::get()->createConfig();
	Application::get()->mainLoop();
}

void nbody::createSphereGeom(const unsigned int stacks, const unsigned int slices, const glm::vec3 &dims)
{

	glm::vec3 colour = glm::vec3(0.7f, 0.7f, 0.7f);// 1.0f);

	// Working values
	float delta_rho = glm::pi<float>() / static_cast<float>(stacks);
	float delta_theta = 2.0f * glm::pi<float>() / static_cast<float>(slices);
	float delta_T = dims.y / static_cast<float>(stacks);
	float delta_S = dims.x / static_cast<float>(slices);
	float t = dims.y;
	float s = 0.0f;

	int ind = 0;

	// Iterate through each stack
	for (unsigned int i = 0; i < stacks; ++i)
	{
		// Set starting values for stack
		float rho = i * delta_rho;
		s = 0.0f;
		// Vertex data generated
		std::array<glm::vec3, 4> verts;
		std::array<glm::vec2, 4> coords;
		// Iterate through each slice
		for (unsigned int j = 0; j < slices; ++j)
		{
			// Vertex 0
			float theta = j * delta_theta;
			verts[0] = glm::vec3(dims.x * -sin(theta) * sin(rho),
				dims.y * cos(theta) * sin(rho),
				dims.z * cos(rho));
			coords[0] = glm::vec2(s, t);
			// Vertex 1
			verts[1] = glm::vec3(dims.x * -sin(theta) * sin(rho + delta_rho),
				dims.y * cos(theta) * sin(rho + delta_rho),
				dims.z * cos(rho + delta_rho));
			coords[1] = glm::vec2(s, t - delta_T);
			// Vertex 2
			theta = ((j + 1) == slices) ? 0.0f : (j + 1) * delta_theta;
			s += delta_S;
			verts[2] = glm::vec3(dims.x * -sin(theta) * sin(rho),
				dims.y * cos(theta) * sin(rho),
				dims.z * cos(rho));
			coords[2] = glm::vec2(s, t);
			// Vertex 3
			verts[3] = glm::vec3(dims.x * -sin(theta) * sin(rho + delta_rho),
				dims.y * cos(theta) * sin(rho + delta_rho),
				dims.z * cos(rho + delta_rho));
			coords[3] = glm::vec2(s, t - delta_T);



			// Triangle 1
			//vertexBuffer.push_back(Vertex(verts[0], colour, coords[0]));
			//vertexBuffer.push_back(Vertex(verts[1], colour, coords[1]));
			//vertexBuffer.push_back(Vertex(verts[2], colour, coords[2]));

			//// tri 2
			//vertexBuffer.push_back(Vertex(verts[1], colour, coords[1]));
			//vertexBuffer.push_back(Vertex(verts[3], colour, coords[3]));
			//vertexBuffer.push_back(Vertex(verts[2], colour, coords[2]));

			//TODO: clean this up
			vertexBuffer.push_back(Vertex(verts[0], colour, coords[0]));
			vertexBuffer.push_back(Vertex(verts[1], colour, coords[1]));
			vertexBuffer.push_back(Vertex(verts[2], colour, coords[2]));
			vertexBuffer.push_back(Vertex(verts[3], colour, coords[3]));

			indexBuffer.push_back(ind + 0);
			indexBuffer.push_back(ind + 1);
			indexBuffer.push_back(ind + 2);
			indexBuffer.push_back(ind + 1);
			indexBuffer.push_back(ind + 3);
			indexBuffer.push_back(ind + 2);

			ind += 4;
		}
		t -= delta_T;
	}

}


nbody::~nbody()
{
	// clean up app
	Application::get()->clean();
}