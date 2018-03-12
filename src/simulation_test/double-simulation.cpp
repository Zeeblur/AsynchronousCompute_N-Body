#include "simulation.h"
#include "renderer.h"

double_simulation::double_simulation(const VkQueue* pQ,
	const VkQueue* gQ,
	const VkDevice* dev) : simulation(pQ, gQ, dev)
{
	compute = new Async();
	renderer = Renderer::get();
}

void double_simulation::createBufferObjects()
{
	for (auto &b : buffers)
	{
		b->createSpecificBuffer();
	}
}

void double_simulation::frame()
{
	//renderer->updateUniformBuffer();   // update
	//computeTransfer();
	//renderer->drawFrame();			   // render
	//renderer->updateCompute();		   // update 
}

void double_simulation::allocateCommandBuffers()
{

}

void double_simulation::recordComputeCommands()
{

}


void double_simulation::cleanup()
{

}
