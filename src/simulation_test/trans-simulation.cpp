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
	renderer->updateCompute();		   // update 


	dispatchCompute();
	renderer->drawFrame();			   // render
	computeTransfer();

}

int trans_simulation::findTransferQueueFamily(VkPhysicalDevice pd)
{
	// find and return the index of compute queue family for device

	int index = -1;

	// as before, find them, set them
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, queueFamilies.data());

	// find suitable family that supports transfer.
	unsigned int i = 0;
	for (const auto& queueFamily : queueFamilies)
	{

		// check for Transfer support 
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
		{
			index = i;
			break;
		}
		i++;
	}

	return index;
}

void trans_simulation::createCommandPools(QueueFamilyIndices& queueFamilyIndices, VkPhysicalDevice& phys)
{
	simulation::createCommandPools(queueFamilyIndices, phys); // call base class

	int queueIndex = findTransferQueueFamily(phys);
	// store value
	vkGetDeviceQueue(device, queueIndex, 0, &transferQueue);

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = queueIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	if (vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &transferPool) != VK_SUCCESS)
		throw std::runtime_error("Failed creating compute cmd pool");
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

	cmdBufAllocateInfo.commandPool = transferPool;
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

	vkCmdResetQueryPool(compute->commandBuffer, renderer->computeQueryPool, 0, 2);
	vkCmdWriteTimestamp(compute->commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, renderer->computeQueryPool, 0);

	// dispatch shader
	vkCmdDispatch(compute->commandBuffer, renderer->PARTICLE_COUNT, 1, 1);

	vkCmdWriteTimestamp(compute->commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, renderer->computeQueryPool, 1);


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
	//cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;


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
	drawBarrier.buffer = buffers[INSTANCE]->buffer[buffIndex + 1]; // draw storage buffer
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
		buffers[INSTANCE]->buffer[buffIndex + 1],
		1,
		&copyRegion);


	// update barrier
	computeBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	computeBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	drawBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	drawBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkBufferMemoryBarrier memBarriersUpdate[] = { computeBarrier, drawBarrier };

	// barrier to transfer
	vkCmdPipelineBarrier(
		transferCmdBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
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
	// Submit to queue asynchronously
	vkResetFences(device, 1, &compute->fence);
	if (vkQueueSubmit(transferQueue, 1, &submitInfo, compute->fence) != VK_SUCCESS)
		throw std::runtime_error("failed to submit transfer queue");

}

void trans_simulation::computeTransfer()
{
	// Check for compute operation results
	if (VK_SUCCESS == vkGetFenceStatus(device, compute->fence))
	{
		copyComputeResults();

	}
}

void trans_simulation::dispatchCompute()
{
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
	computeSubmitInfo.pCommandBuffers = &compute->commandBuffer;
	computeSubmitInfo.commandBufferCount = 1;

	if (vkQueueSubmit(compute->queue, 1, &computeSubmitInfo, compute->fence) != VK_SUCCESS)
		throw std::runtime_error("failed to submit compute queue");
}

void trans_simulation::cleanup()
{
	vkDestroyCommandPool(device, transferPool, nullptr);
	compute->cleanup(device);
}
