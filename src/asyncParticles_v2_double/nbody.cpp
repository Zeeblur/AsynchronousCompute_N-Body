#include "nbody.h"

using namespace glm;

// TODO: define vars for window/particles etc
nbody::nbody(const unsigned int num)
{
	// initialise the application
	num_particles = num; 
	initialiseVulkan();
}

void nbody::initialiseVulkan()
{
	auto &app = Application::get();

	app->init();

}

void nbody::prepareParticles()
{
	particleBuffer.resize(num_particles);

	// create positions for particles 
	for (auto &p : particleBuffer)
	{
		//set rnd position
		float v1 = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 20));
		v1 -= 10;

		auto v2 = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 20));
		v2 -= 10;

		auto v3 = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 20));
		v3 -= 10;

		auto massRNG = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 200));

		auto mass = 1.0;

		if (massRNG < 150)
			mass = 1.0;

		p.pos = vec4(v1, v2, 0, 100);
		p.vel = vec4(0.0);

		auto rndM = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 200));
	}
}

void nbody::run()
{
	// loop here  
	prepareParticles();     
	createSphereGeom(20, 20, vec3(0.02f));
	Application::get()->setVertexData(vertexBuffer, indexBuffer, particleBuffer);

	// create config sets up the storage buffers for the data and uniforms. 
	// creates the descriptions and command buffers.
	Application::get()->createConfig(num_particles);
	Application::get()->mainLoop();
}

void nbody::createSphereGeom(const unsigned int stacks, const unsigned int slices, const glm::vec3 &dims)
{

	glm::vec3 colour = glm::vec3(0.7f, 0.7f, 0.7f);

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