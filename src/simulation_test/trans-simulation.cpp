#include "simulation.h"
#include "renderer.h"

trans_simulation::trans_simulation(const VkQueue* pQ,
	const VkQueue* gQ,
	const VkDevice* dev) : simulation(pQ, gQ, dev)
{
	compute = new ComputeConfig();
	renderer = Renderer::get();
}

void trans_simulation::createBufferObjects()
{
	for (auto &b : buffers)
	{
		b->createSpecificBuffer();
	}

	// create second buffer for draw storage transfers
	auto buffer = dynamic_cast<InstanceBO*>(buffers[INSTANCE]);
	buffer->createDrawStorage();
}

void trans_simulation::frame()
{
	renderer->updateUniformBuffer();   // update
	computeTransfer();
	dispatchCompute();
	renderer->drawFrame();			   // render
	renderer->updateCompute();		   // update 
}

// compute & transfer command buffers
void trans_simulation::allocateComputeCommandBuffers()
{
	// Create a command buffer for compute operations
	VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = compute->commandPool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 1;
	if (vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &compute->commandBuffer) != VK_SUCCESS)
		throw std::runtime_error("Failed allocating buffer for compute commands");
	// allocate transfer command buffer
	if (vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &transferCmdBuffer) != VK_SUCCESS)
		throw std::runtime_error("Failed allocating buffer for transfer commands");

	// Fence for compute CB sync
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	if (vkCreateFence(device, &fenceCreateInfo, nullptr, &compute->fence) != VK_SUCCESS)
		throw std::runtime_error("Failed creating compute fence");
}

void trans_simulation::recordComputeCommands()
{
	// create command buffer
	// Compute: Begin, bind pipeline, bind desc sets, dispatch calls, end.

	// begin
	VkCommandBufferBeginInfo cmdBufInfo{};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(compute->commandBuffer, &cmdBufInfo) != VK_SUCCESS)
		throw std::runtime_error("Compute command buffer failed to start");

	// bind pipeline & desc sets
	vkCmdBindPipeline(compute->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline);
	vkCmdBindDescriptorSets(compute->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipelineLayout, 0, 1, &compute->descriptorSet, 0, 0);

	// dispatch shader
	vkCmdDispatch(compute->commandBuffer, renderer->PARTICLE_COUNT, 1, 1);

	// end cmd writing
	vkEndCommandBuffer(compute->commandBuffer);

	// Set up memory barriers
	// need for compute and draw

	recordTransferCommands();
}

void trans_simulation::recordTransferCommands()
{
	// create commandBuffer
	VkCommandBufferBeginInfo cmdBufInfo{};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkBufferMemoryBarrier computeBarrier, drawBarrier;
	computeBarrier.srcQueueFamilyIndex = 0;
	computeBarrier.dstQueueFamilyIndex = 0;
	computeBarrier.offset = 0;
	computeBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	computeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
	computeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	computeBarrier.buffer = buffers[INSTANCE]->buffer[buffIndex]; // comput storgae buffer
	computeBarrier.size = buffers[INSTANCE]->size * sizeof(particle); // desc range

	computeBarrier.pNext = nullptr;
	drawBarrier.pNext = nullptr;

	// draw barrier
	drawBarrier.srcQueueFamilyIndex = 0;
	drawBarrier.dstQueueFamilyIndex = 0;
	drawBarrier.offset = 0;
	drawBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	drawBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	drawBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	drawBarrier.buffer = buffers[INSTANCE]->buffer[buffIndex+1]; // draw storage buffer
	drawBarrier.size = buffers[INSTANCE]->size * sizeof(particle);

	// begin writing to transfer cmd buffer
	// set up pipeline barrier, copy buffer, change barriers, end.
	if (vkBeginCommandBuffer(transferCmdBuffer, &cmdBufInfo) != VK_SUCCESS)
		throw std::runtime_error("transfer command buffer failed to start");


	VkBufferMemoryBarrier memBarriers[] = { computeBarrier, drawBarrier };

	// barrier to transfer
	vkCmdPipelineBarrier(
		transferCmdBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, // no flags
		0, nullptr,
		2, memBarriers,
		0, nullptr);

	// Copy buffers
	VkBufferCopy copyRegion = {};
	copyRegion.size = buffers[INSTANCE]->size * sizeof(particle);
	vkCmdCopyBuffer(transferCmdBuffer,
		buffers[INSTANCE]->buffer[buffIndex],   // copy from storage to draw
		buffers[INSTANCE]->buffer[buffIndex+1],
		1,
		&copyRegion);


	// update barrier
	computeBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	computeBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	drawBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	drawBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

	VkBufferMemoryBarrier memBarriersUpdate[] = { computeBarrier, drawBarrier };

	// barrier to transfer
	vkCmdPipelineBarrier(
		transferCmdBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
		0, // no flags
		0, nullptr,
		2, memBarriersUpdate,
		0, nullptr);

	// end tra writing
	vkEndCommandBuffer(transferCmdBuffer);
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
	if (vkQueueSubmit(compute->queue, 1, &submitInfo, fence) != VK_SUCCESS)
		throw std::runtime_error("failed to submit transfer queue");

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
}

void trans_simulation::dispatchCompute()
{
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

void trans_simulation::cleanup()
{
	compute->cleanup(device);
}
