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

	// create second buffer for draw storage transfers
	auto buffer = dynamic_cast<InstanceBO*>(buffers[INSTANCE]);
	buffer->createDrawStorage();
}

void double_simulation::frame()
{
	renderer->updateUniformBuffer();   // update
	renderer->updateCompute();		   // update	
	dispatchCompute();				   // submit compute
	renderer->drawFrame();			   // render	  

	waitOnFence(renderer->graphicsFence);
	bufferIndex = 1 - bufferIndex;

	waitOnFence(compute->fence);
}

void double_simulation::createDescriptorPool()
{
	// how many and what type
	std::vector<VkDescriptorPoolSize> poolSize = { VkDescriptorPoolSize(), VkDescriptorPoolSize(), VkDescriptorPoolSize() };
	poolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize[0].descriptorCount = 3;
	poolSize[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize[1].descriptorCount = 2;
	poolSize[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize[2].descriptorCount = 1;


	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSize.size();
	poolInfo.pPoolSizes = poolSize.data();
	poolInfo.maxSets = 5; // max sets to allocate

						  // create it
	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &renderer->descriptorPool) != VK_SUCCESS)
		throw std::runtime_error("failed to create descriptor pool!");

}

void double_simulation::createDescriptorSets()
{
	// have to cast compute to use multi buffers
	auto comp = static_cast<Async*>(compute);

	// allocate descriptor set info
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = renderer->descriptorPool;
	allocInfo.pSetLayouts = &comp->descriptorSetLayout;
	allocInfo.descriptorSetCount = 1;

	// for double buffering
	for (int i = 0; i < 2; i++)
	{
		// create allocation
		if (vkAllocateDescriptorSets(device, &allocInfo, &comp->descriptorSet[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to allocate descriptor set for compute");

		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = buffers[INSTANCE]->buffer[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(particle) * buffers[INSTANCE]->size;  //  BUFFER SIZE FOR COMPUTE!

		VkDescriptorBufferInfo UBI = {};
		UBI.buffer = comp->uniformBuffer;
		UBI.offset = 0;
		UBI.range = sizeof(ComputeConfig::computeUBO);

		// Binding 0 : Particle position storage buffer
		VkWriteDescriptorSet storageDesc{};
		storageDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		storageDesc.dstSet = comp->descriptorSet[i];
		storageDesc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		storageDesc.dstBinding = 0;
		storageDesc.pBufferInfo = &bufferInfo;
		storageDesc.descriptorCount = 1;

		// Binding 1 : Uniform buffer
		VkWriteDescriptorSet uniformDesc{};
		uniformDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uniformDesc.dstSet = comp->descriptorSet[i];
		uniformDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniformDesc.dstBinding = 1;
		uniformDesc.pBufferInfo = &UBI;
		uniformDesc.descriptorCount = 1;

		std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = { storageDesc, uniformDesc };

		// create sets
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, NULL);

	}
}

void double_simulation::allocateComputeCommandBuffers()
{
	auto comp = static_cast<Async*>(compute);

	// Create a command buffer for compute operations
	VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = comp->commandPool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 2;
	if (vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, comp->commandBuffer) != VK_SUCCESS)
		throw std::runtime_error("Failed allocating buffer for compute commands");
	// Fence for compute CB sync
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = 0;
	if (vkCreateFence(device, &fenceCreateInfo, nullptr, &compute->fence) != VK_SUCCESS)
		throw std::runtime_error("Failed creating compute fence");

	// fence for gfx
	if (vkCreateFence(device, &fenceCreateInfo, nullptr, &renderer->graphicsFence) != VK_SUCCESS)
		throw std::runtime_error("Failed creating gfx fence");
}

void double_simulation::allocateGraphicsCommandBuffers()
{
	// allocate the buffers
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = renderer->gfxCommandPool;

	// primary can be submitted to a queue for execution but cannot be called from other command buffers
	// secondary cannot be submitted but can be called from primary buffers
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 2;

	if (vkAllocateCommandBuffers(device, &allocInfo, renderer->graphicsCmdBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}
}

void double_simulation::recordComputeCommands()
{
	recordComputeCommand(0);
	recordComputeCommand(1);
}

void double_simulation::recordComputeCommand(int frame)
{
	// create command buffer For current frame....
	// Compute: Begin, bind pipeline, bind desc sets, dispatch calls, end.
	auto comp = static_cast<Async*>(compute);

	// begin
	VkCommandBufferBeginInfo cmdBufInfo{};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(comp->commandBuffer[frame], &cmdBufInfo) != VK_SUCCESS)
		throw std::runtime_error("Compute command buffer failed to start");

	// bind pipeline & desc sets
	vkCmdBindPipeline(comp->commandBuffer[frame], VK_PIPELINE_BIND_POINT_COMPUTE, comp->pipeline);

	vkCmdBindDescriptorSets(comp->commandBuffer[frame], VK_PIPELINE_BIND_POINT_COMPUTE, comp->pipelineLayout, 0, 1, &comp->descriptorSet[frame], 0, nullptr);
	//vkCmdResetQueryPool(comp->commandBuffer[frame], computeQueryPool, 0, 2);
	//vkCmdWriteTimestamp(comp->commandBuffer[frame], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, computeQueryPool, 0);
	vkCmdDispatch(comp->commandBuffer[frame], renderer->PARTICLE_COUNT, 1, 1);
	//vkCmdWriteTimestamp(comp->commandBuffer[frame], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, computeQueryPool, 1);


	// end cmd writing
	vkEndCommandBuffer(comp->commandBuffer[frame]);
}

void double_simulation::recordGraphicsCommands()
{
	// resize to 2
	renderer->graphicsCmdBuffers.resize(2);
	allocateGraphicsCommandBuffers();
	recordRenderCommand(0);
	recordRenderCommand(1);
}

void double_simulation::recordRenderCommand(int frame)
{
	// begin recording command buffer
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	// command buffer can be resubmitted whilst already pending execution - so can scheduling drawing for next frame whilst last frame isn't finished
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr; // Optional - only relevant for secondary cmd buffers

										  // this call resets command buffer as not possible to ammend
	vkBeginCommandBuffer(renderer->graphicsCmdBuffers[frame], &beginInfo);

	// Start the render pass
	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	// render pass and it's attachments to bind (in this case a colour attachment from the frambuffer)
	renderPassInfo.renderPass = renderer->renderPass;
	renderPassInfo.framebuffer = renderer->swapChainFramebuffers[frame];

	// size of render area
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = renderer->swapChainExtent;

	// set clear colour vals
	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	// begin pass - command buffer to record to, the render pass details, how the commands are provided (from 1st/2ndary)
	vkCmdBeginRenderPass(renderer->graphicsCmdBuffers[frame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	// BIND THE PIPELINE
	vkCmdBindPipeline(renderer->graphicsCmdBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->graphicsPipeline);

	// bind the vbo
	VkBuffer vertexBuffers[] = { buffers[VERTEX]->buffer[buffIndex] };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(renderer->graphicsCmdBuffers[frame], 0, 1, vertexBuffers, offsets); // vbo

	VkBuffer instanceBuffers[] = { buffers[INSTANCE]->buffer[frame] };

	vkCmdBindVertexBuffers(renderer->graphicsCmdBuffers[frame], 1, 1, instanceBuffers, offsets); // instance
																				   // bind index & uniforms
	vkCmdBindIndexBuffer(renderer->graphicsCmdBuffers[frame], buffers[INDEX]->buffer[buffIndex], 0, VK_INDEX_TYPE_UINT16);
	vkCmdBindDescriptorSets(renderer->graphicsCmdBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->pipelineLayout, 0, 1, &renderer->gfxDescriptorSet, 0, nullptr);

	// DRAW A TRIANGLEEEEE!!
	// vertex count, instance count, first vertex/ first instance. - used for offsets
	vkCmdDrawIndexed(renderer->graphicsCmdBuffers[frame], static_cast<uint32_t>(buffers[INDEX]->size), static_cast<uint32_t>(buffers[INSTANCE]->size), 0, 0, 0);
	//vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(vertices.size()), 1, 0, 0);

	// end the pass
	vkCmdEndRenderPass(renderer->graphicsCmdBuffers[frame]);

	// check if failed recording
	if (vkEndCommandBuffer(renderer->graphicsCmdBuffers[frame]) != VK_SUCCESS)
		throw std::runtime_error("failed to record command buffer!");

}

void double_simulation::waitOnFence(VkFence& fence)
{
	// spinlock on fence
	while (vkWaitForFences(device, 1, &fence, VK_TRUE, 1000) != VK_SUCCESS);

	// reset fence
	vkResetFences(device, 1, &fence);
}

void double_simulation::dispatchCompute()
{
	// have to cast compute to use multi buffers
	auto comp = static_cast<Async*>(compute);

	VkSubmitInfo computeSubmitInfo{};
	computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	computeSubmitInfo.pCommandBuffers = &comp->commandBuffer[bufferIndex];
	computeSubmitInfo.commandBufferCount = 1;

	if (vkQueueSubmit(comp->queue, 1, &computeSubmitInfo, comp->fence) != VK_SUCCESS)
		throw std::runtime_error("failed to submit compute queue");


}

void double_simulation::cleanup()
{
	compute->cleanup(device);
}
