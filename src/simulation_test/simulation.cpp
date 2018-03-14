#include "simulation.h"
#include "renderer.h"
#include "compute.h"

simulation::~simulation() {}

void simulation::createCommandPools(QueueFamilyIndices& queueFamilyIndices, VkPhysicalDevice& phys)
{
	// record commands for drawing on the graphics queue
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	// create the pool
	if (vkCreateCommandPool(device, &poolInfo, nullptr, &renderer->gfxCommandPool) != VK_SUCCESS)
		throw std::runtime_error("failed to create gfx command pool!");

	// create compute command pool
	if (queueFamilyIndices.graphicsFamily == queueFamilyIndices.computeFamily)
	{
		compute->commandPool = renderer->gfxCommandPool;
	}
	else 	// Separate command pool as queue family for compute may be different than graphics
	{
		poolInfo.queueFamilyIndex = queueFamilyIndices.computeFamily;

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &compute->commandPool) != VK_SUCCESS)
			throw std::runtime_error("Failed creating compute cmd pool");

	}
}

void simulation::recordGraphicsCommands()
{

	// resize allocation for frame buffers
	renderer->graphicsCmdBuffers.resize(renderer->swapChainFramebuffers.size());

	// allocate the buffers
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = renderer->gfxCommandPool;

	// primary can be submitted to a queue for execution but cannot be called from other command buffers
	// secondary cannot be submitted but can be called from primary buffers
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)renderer->graphicsCmdBuffers.size();

	if (vkAllocateCommandBuffers(device, &allocInfo, renderer->graphicsCmdBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}

	// record into command buffers
	for (size_t i = 0; i < renderer->graphicsCmdBuffers.size(); i++)
	{
		// begin recording command buffer
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		// command buffer can be resubmitted whilst already pending execution - so can scheduling drawing for next frame whilst last frame isn't finished
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		beginInfo.pInheritanceInfo = nullptr; // Optional - only relevant for secondary cmd buffers

											  // this call resets command buffer as not possible to ammend
		vkBeginCommandBuffer(renderer->graphicsCmdBuffers[i], &beginInfo);

		// Start the render pass
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		// render pass and it's attachments to bind (in this case a colour attachment from the frambuffer)
		renderPassInfo.renderPass = renderer->renderPass;
		renderPassInfo.framebuffer = renderer->swapChainFramebuffers[i];

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
		vkCmdBeginRenderPass(renderer->graphicsCmdBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// BIND THE PIPELINE
		vkCmdBindPipeline(renderer->graphicsCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->graphicsPipeline);

		// bind the vbo
		VkBuffer vertexBuffers[] = { buffers[VERTEX]->buffer[buffIndex] };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(renderer->graphicsCmdBuffers[i], 0, 1, vertexBuffers, offsets); // vbo

		VkBuffer instanceBuffers[] = { buffers[INSTANCE]->buffer[buffIndex] };

		vkCmdBindVertexBuffers(renderer->graphicsCmdBuffers[i], 1, 1, instanceBuffers, offsets); // instance
																								 // bind index & uniforms
		vkCmdBindIndexBuffer(renderer->graphicsCmdBuffers[i], buffers[INDEX]->buffer[buffIndex], 0, VK_INDEX_TYPE_UINT16);
		vkCmdBindDescriptorSets(renderer->graphicsCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->pipelineLayout, 0, 1, &renderer->gfxDescriptorSet, 0, nullptr);

		// DRAW A TRIANGLEEEEE!!!?"!?!!?!?!?!?!?!
		// vertex count, instance count, first vertex/ first instance. - used for offsets
		vkCmdDrawIndexed(renderer->graphicsCmdBuffers[i], static_cast<uint32_t>(buffers[INDEX]->size), static_cast<uint32_t>(buffers[INSTANCE]->size), 0, 0, 0);
		//vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(vertices.size()), 1, 0, 0);

		// end the pass
		vkCmdEndRenderPass(renderer->graphicsCmdBuffers[i]);

		// check if failed recording
		if (vkEndCommandBuffer(renderer->graphicsCmdBuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to record command buffer!");

	}

}

// create descriptor pool to create them (liuke command buffers)
// specifies a buffer resource to bind the uniform descriptor to
void simulation::createDescriptorPool()
{
	// how many and what type
	std::vector<VkDescriptorPoolSize> poolSize = { VkDescriptorPoolSize(), VkDescriptorPoolSize(), VkDescriptorPoolSize() };
	poolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize[0].descriptorCount = 2;
	poolSize[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize[1].descriptorCount = 1;
	poolSize[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize[2].descriptorCount = 1;


	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSize.size();
	poolInfo.pPoolSizes = poolSize.data();
	poolInfo.maxSets = 2; // max sets to allocate

	// create it
	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &renderer->descriptorPool) != VK_SUCCESS)
		throw std::runtime_error("failed to create descriptor pool!");

}

void simulation::createDescriptorSets()
{
	// allocate descriptor set info
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = renderer->descriptorPool;
	allocInfo.pSetLayouts = &compute->descriptorSetLayout;
	allocInfo.descriptorSetCount = 1;

	// create allocation
	if (vkAllocateDescriptorSets(device, &allocInfo, &compute->descriptorSet) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate descriptor set for compute");

	// create buffers.
	//compute->storageBuffer = buffers[INSTANCE];

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = buffers[INSTANCE]->buffer[0];
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(particle) * buffers[INSTANCE]->size;  //  BUFFER SIZE FOR COMPUTE!


	VkDescriptorBufferInfo UBI = {};
	UBI.buffer = compute->uniformBuffer;
	UBI.offset = 0;
	UBI.range = sizeof(ComputeConfig::computeUBO);

	// Binding 0 : Particle position storage buffer
	VkWriteDescriptorSet storageDesc{};
	storageDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	storageDesc.dstSet = compute->descriptorSet;
	storageDesc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	storageDesc.dstBinding = 0;
	storageDesc.pBufferInfo = &bufferInfo;
	storageDesc.descriptorCount = 1;

	// Binding 1 : Uniform buffer
	VkWriteDescriptorSet uniformDesc{};
	uniformDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	uniformDesc.dstSet = compute->descriptorSet;
	uniformDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniformDesc.dstBinding = 1;
	uniformDesc.pBufferInfo = &UBI;
	uniformDesc.descriptorCount = 1;

	std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = { storageDesc, uniformDesc };

	// create sets
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, NULL);

}