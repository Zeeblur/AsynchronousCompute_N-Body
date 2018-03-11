#include "simulation.h"
#include "renderer.h"
#include "compute.h"

simulation::~simulation() {}

comp_simulation::comp_simulation(const VkQueue* pQ,
	const VkQueue* gQ,
	const VkDevice* dev) : simulation(pQ, gQ, dev)
{
	compute = new ComputeConfig();
	renderer = Renderer::get();
}


void comp_simulation::frame()
{
	renderer->updateUniformBuffer();   // update
	renderer->drawFrame();			   // render
	dispatchCompute();
	renderer->updateCompute();		 // up

}

void comp_simulation::dispatchCompute()
{
	// wait until presentation is finished before drawing the next frame
	vkQueueWaitIdle(presentQueue);

	auto fenceResult = vkWaitForFences(device, 1, &compute->fence, VK_TRUE, UINT64_MAX);
	// Submit compute commands
	while (fenceResult != VK_SUCCESS)
	{
		if (fenceResult == VK_ERROR_DEVICE_LOST)
			throw std::runtime_error("device crashed");

		fenceResult = vkWaitForFences(device, 1, &compute->fence, VK_TRUE, UINT64_MAX);
	};

	vkResetFences(device, 1, &compute->fence);
	VkSubmitInfo computeSubmitInfo{};
	computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	computeSubmitInfo.commandBufferCount = 1;
	computeSubmitInfo.pCommandBuffers = &compute->commandBuffer;

	auto re = vkQueueSubmit(compute->queue, 1, &computeSubmitInfo, compute->fence);
	if (re != VK_SUCCESS)
	{
		std::cout << "result: " << re << std::endl;
		throw std::runtime_error("failed to submit compute queue");
	}
	
}

void comp_simulation::createCommandBuffers()
{

}

void comp_simulation::cleanup()
{

}