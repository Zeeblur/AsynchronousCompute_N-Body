#include "simulation.h"
#include "renderer.h"

trans_simulation::trans_simulation(const VkQueue* pQ,
	const VkQueue* gQ,
	const VkDevice* dev) : simulation(pQ, gQ, dev)
{
	//compute = new ComputeConfig();
}

void trans_simulation::frame()
{
	renderer->updateUniformBuffer();   // update
	computeTransfer();
	renderer->drawFrame();			   // render
	renderer->updateCompute();		   // update 
}

// copy compute results
void trans_simulation::copyComputeResults()
{

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCmdBuffer;
	// Submit to queue (maybe graphics?)
	VkFence fence = VK_NULL_HANDLE;
	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS)
		throw std::runtime_error("failed to submit gfx queue");

}


void trans_simulation::computeTransfer()
{
	// Check for compute operation results
	if (compute->fence && VK_SUCCESS == vkGetFenceStatus(device, compute->fence))
	{
		copyComputeResults();
		vkDestroyFence(device, compute->fence, nullptr);
		//compute->fence = nullptr;// new fence;
		VkFence newFence = VK_NULL_HANDLE;
		compute->fence = newFence;
	}

	if (!compute->fence)
	{
		// create fence
		// Fence for compute CB sync
		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		//fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		if (vkCreateFence(device, &fenceCreateInfo, nullptr, &compute->fence) != VK_SUCCESS)
			throw std::runtime_error("Failed creating compute fence");


		VkSubmitInfo computeSubmitInfo{};
		computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		computeSubmitInfo.pCommandBuffers = &compute->commandBuffer;
		computeSubmitInfo.commandBufferCount = 1;

		if (vkQueueSubmit(compute->queue, 1, &computeSubmitInfo, compute->fence) != VK_SUCCESS)
			throw std::runtime_error("failed to submit compute queue");
	}
}

void trans_simulation::createCommandBuffers()
{

}

void trans_simulation::cleanup()
{

}
