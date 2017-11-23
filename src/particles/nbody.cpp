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

	Application::get()->createVertexBuffer(createSphereGeom(20, 20, vec3(0.5f)));
	Application::get()->createConfig();
	Application::get()->mainLoop();
}

std::vector<Vertex> nbody::createSphereGeom(const unsigned int stacks, const unsigned int slices, glm::vec3 &dims)
{

	auto olddim = dims.y;
	dims.y = 1.0f;

	std::vector<Vertex> verticesSphere;

	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> tex_coords;
	glm::vec3 colour = glm::vec3(0.7f, 0.7f, 0.7f);// 1.0f);
	// Minimal and maximal points
	glm::vec3 minimal(0.0f, 0.0f, 0.0f);
	glm::vec3 maximal(0.0f, 0.0f, 0.0f);
	// Working values
	float delta_rho = glm::pi<float>() / static_cast<float>(stacks);
	float delta_theta = 2.0f * glm::pi<float>() / static_cast<float>(slices);
	float delta_T = dims.y / static_cast<float>(stacks);
	float delta_S = dims.x / static_cast<float>(slices);
	float t = dims.y;
	float s = 0.0f;

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

			// Recalculate minimal and maximal
			for (auto &v : verts)
			{
				minimal = glm::min(minimal, v);
				maximal = glm::max(maximal, v);
			}

			// Triangle 1

			verticesSphere.push_back(Vertex(verts[0], colour, coords[0]));
			verticesSphere.push_back(Vertex(verts[1], colour, coords[1]));
			verticesSphere.push_back(Vertex(verts[2], colour, coords[2]));

			// tri 2
			verticesSphere.push_back(Vertex(verts[1], colour, coords[1]));
			verticesSphere.push_back(Vertex(verts[3], colour, coords[3]));
			verticesSphere.push_back(Vertex(verts[2], colour, coords[2]));


			//positions.push_back(verts[0]);
			//normals.push_back(glm::normalize(verts[0]));
			//tex_coords.push_back(coords[0]);
			//positions.push_back(verts[1]);
			//normals.push_back(glm::normalize(verts[1]));
			//tex_coords.push_back(coords[1]);
			//positions.push_back(verts[2]);
			//normals.push_back(glm::normalize(verts[2]));
			//tex_coords.push_back(coords[2]);

			//// Triangle 2
			//positions.push_back(verts[1]);
			//normals.push_back(glm::normalize(verts[1]));
			//tex_coords.push_back(coords[1]);
			//positions.push_back(verts[3]);
			//normals.push_back(glm::normalize(verts[3]));
			//tex_coords.push_back(coords[3]);
			//positions.push_back(verts[2]);
			//normals.push_back(glm::normalize(verts[2]));
			//tex_coords.push_back(coords[2]);
		}
		t -= delta_T;
	}

	dims.x = 2.0f;
	dims.y = olddim;
	// Working values
delta_rho = glm::pi<float>() / static_cast<float>(stacks);
delta_theta = 2.0f * glm::pi<float>() / static_cast<float>(slices);
delta_T = dims.y / static_cast<float>(stacks);
delta_S = dims.x / static_cast<float>(slices);
t = dims.y;
s = 0.0f;

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

			// Recalculate minimal and maximal
			for (auto &v : verts)
			{
				minimal = glm::min(minimal, v);
				maximal = glm::max(maximal, v);
			}

			// Triangle 1

			verticesSphere.push_back(Vertex(verts[0], colour, coords[0]));
			verticesSphere.push_back(Vertex(verts[1], colour, coords[1]));
			verticesSphere.push_back(Vertex(verts[2], colour, coords[2]));

			// tri 2
			verticesSphere.push_back(Vertex(verts[1], colour, coords[1]));
			verticesSphere.push_back(Vertex(verts[3], colour, coords[3]));
			verticesSphere.push_back(Vertex(verts[2], colour, coords[2]));


			//positions.push_back(verts[0]);
			//normals.push_back(glm::normalize(verts[0]));
			//tex_coords.push_back(coords[0]);
			//positions.push_back(verts[1]);
			//normals.push_back(glm::normalize(verts[1]));
			//tex_coords.push_back(coords[1]);
			//positions.push_back(verts[2]);
			//normals.push_back(glm::normalize(verts[2]));
			//tex_coords.push_back(coords[2]);

			//// Triangle 2
			//positions.push_back(verts[1]);
			//normals.push_back(glm::normalize(verts[1]));
			//tex_coords.push_back(coords[1]);
			//positions.push_back(verts[3]);
			//normals.push_back(glm::normalize(verts[3]));
			//tex_coords.push_back(coords[3]);
			//positions.push_back(verts[2]);
			//normals.push_back(glm::normalize(verts[2]));
			//tex_coords.push_back(coords[2]);
		}
		t -= delta_T;
	}


	// Add buffers to geometry
	//geom.add_buffer(positions, BUFFER_INDEXES::POSITION_BUFFER);
	//geom.add_buffer(colours, BUFFER_INDEXES::COLOUR_BUFFER);
	//geom.add_buffer(tex_coords, BUFFER_INDEXES::TEXTURE_COORDS_0);

	return verticesSphere;
}


nbody::~nbody()
{
	// clean up app
	Application::get()->clean();
}