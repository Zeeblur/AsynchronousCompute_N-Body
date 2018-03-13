#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct BufferObject;

struct ComputeConfig
{
	BufferObject* storageBuffer;					// (Shader) storage buffer object containing the particles
	VkBuffer uniformBuffer;		    // Uniform buffer object containing particle system parameters
	VkQueue queue;								// Separate queue for compute commands (queue family may differ from the one used for graphics)
	VkCommandPool commandPool;					// Use a separate command pool (queue family may differ from the one used for graphics)
	VkCommandBuffer commandBuffer;				// Command buffer storing the dispatch commands and barriers
	VkFence fence;								// Synchronization fence to avoid rewriting compute CB if still in use
	VkDescriptorSetLayout descriptorSetLayout;	// Compute shader binding layout
	VkDescriptorSet descriptorSet;				// Compute shader bindings
	VkPipelineLayout pipelineLayout;			// Layout of the compute pipeline
	VkPipeline pipeline;						// Compute pipeline for updating particle positions

												// memory for ubo
	VkDeviceMemory uboMem;
	void* mapped = nullptr;
	// Compute shader uniform block object
	struct computeUBO
	{
		float deltaT;							//		Frame delta time
		float destX;							//		x position of the attractor
		float destY;							//		y position of the attractor
		int32_t particleCount = 0;
	} ubo;

	ComputeConfig();

	void cleanup(VkDevice& device)
	{
		// compute clean up
		vkDestroyFence(device, fence, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(1), &commandBuffer);
		vkDestroyCommandPool(device, commandPool, nullptr);
		vkDestroyBuffer(device, uniformBuffer, nullptr);
		vkFreeMemory(device, uboMem, nullptr);
	}
};


struct Async : public ComputeConfig
{
	VkCommandBuffer commandBuffer[2];			// 2 Command buffer storing the dispatch commands and barriers
	VkDescriptorSet descriptorSet[2];			// Compute shader bindings (FOR 1 AND 2)!!!!!
};