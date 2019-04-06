#include "nbody.h"

using namespace glm;

nbody::nbody(const unsigned int num, const bool AMD, const MODE chosenMode)
{
	// initialise the Renderer
	num_particles = num; 

	// initialise vulkan
	auto &app = Renderer::get();
	app->init(chosenMode, AMD);
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

		auto massRNG = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 200));

		auto mass = 1.0;

		if (massRNG < 150)
			mass = 1.0;

		p.pos = vec4(v1, v2, 0.0f, 100);
		p.vel = vec4(0.0);

		auto rndM = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 200));
	}
}

void nbody::run(const parameters simParam)
{
	// loop here  
	prepareParticles();     
	createSphereGeom(simParam.stacks, simParam.slices, simParam.dims);
	Renderer::get()->setVertexData(vertexBuffer, indexBuffer, particleBuffer);
	// create config sets up the storage buffers for the data and uniforms. 
	// creates the descriptions and command buffers.

	Renderer::get()->createConfig(simParam);
	Renderer::get()->mainLoop();
}

void nbody::createSphereGeom(const unsigned int stacks, const unsigned int slices, const glm::vec3 dims)
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
			s += delta_S;
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
			s = 0;
			verts[2] = glm::vec3(dims.x * -sin(theta) * sin(rho),
				dims.y * cos(theta) * sin(rho),
				dims.z * cos(rho));
			coords[2] = glm::vec2(s, t);
			// Vertex 3
			verts[3] = glm::vec3(dims.x * -sin(theta) * sin(rho + delta_rho),
				dims.y * cos(theta) * sin(rho + delta_rho),
				dims.z * cos(rho + delta_rho));
			coords[3] = glm::vec2(s, t - delta_T);

			for (auto &uv : coords)
			{
				uv = vec2(uv.x * 0.2, uv.y * 0.2);
			}

			//TODO: clean this up
			auto v0 = Vertex(verts[0], glm::normalize(verts[0]), coords[0]);
			auto v1 = Vertex(verts[1], glm::normalize(verts[1]), coords[1]);
			auto v2 = Vertex(verts[2], glm::normalize(verts[2]), coords[2]);
			auto v3 = Vertex(verts[3], glm::normalize(verts[3]), coords[3]);

			std::vector<Vertex*> triangle1 = { &v0, &v1, &v2 };
			std::vector<Vertex*> triangle2 = { &v1, &v3, &v2 };

			std::map <Vertex*, std::vector<glm::vec3>> tangents;
			std::map <Vertex*, std::vector<glm::vec3>> bitangents;

			computeTangentBasis(triangle1, tangents, bitangents);
			computeTangentBasis(triangle2, tangents, bitangents);

			// average tangents for vertices with multiple triangles.
			for (auto const& x : tangents)
			{
				auto key = x.first;
				auto val = x.second;

				auto size = val.size();

				glm::vec3 average = glm::vec3(0.0);
				for (int i = 0; i < size; ++i)
				{
					average += val[i];
				}

				average /= size;

				key->tangent = average;

			}

			for (auto const& x : bitangents)
			{
				auto key = x.first;
				auto val = x.second;

				auto size = val.size();

				glm::vec3 average = glm::vec3(0.0);
				for (int i = 0; i < size; ++i)
				{
					average += val[i];
				}

				average /= size;

				key->bitangent = average;

			}

			vertexBuffer.push_back(v0);
			vertexBuffer.push_back(v1);
			vertexBuffer.push_back(v2);
			vertexBuffer.push_back(v3);

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

// calculate tangents and binormals per triangle
void nbody::computeTangentBasis(std::vector<Vertex*> &triangleVertices, std::map <Vertex*, std::vector<glm::vec3>> &tangents, std::map <Vertex*, std::vector<glm::vec3>> &bitangents)
{
	auto v0 = triangleVertices[0];
	auto v1 = triangleVertices[1];
	auto v2 = triangleVertices[2];

	// calculate edges of the triangle
	glm::vec3 deltaPos1 = v1->pos - v0->pos;
	glm::vec3 deltaPos2 = v2->pos - v0->pos;

	// UV delta
	glm::vec2 deltaUV1 = v1->texCoord - v0->texCoord;
	glm::vec2 deltaUV2 = v2->texCoord - v0->texCoord;

	float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);

	glm::vec3 tangent = glm::vec3((v1->pos.x * v2->texCoord.y - v2->pos.x * v1->texCoord.y) * r,
		(v1->pos.y * v2->texCoord.y - v2->pos.y * v1->texCoord.y) * r,
		(v1->pos.z * v2->texCoord.y - v2->pos.z * v1->texCoord.y) * r);

	glm::vec3 bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x)*r;

	for (auto &v : triangleVertices)
	{
		const vec3& n = v->normal;
		const vec3& t = tangent;

		// Gram-Schmidt orthogonalize
		glm::vec3 calcTangent = glm::normalize(t - (n * glm::dot(n, t)));
		assert(!glm::isnan(calcTangent.x));

		// Calculate handedness
		if (glm::dot(glm::cross(n, t), bitangent) < 0.0f)
		{
			calcTangent *= -1.0f;
		}
		assert(!glm::isnan(calcTangent.x));
		tangents[v].push_back(calcTangent);
		bitangents[v].push_back(bitangent);
	}
}

nbody::~nbody()
{
	// clean up app
	Renderer::get()->clean();
}