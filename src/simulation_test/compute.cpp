#include "compute.h"
#include "buffer.h"

ComputeConfig::ComputeConfig()
{

}

void ComputeConfig::cleanup(const VkDevice& device)
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

void Async::cleanup(const VkDevice& device)
{
	// compute clean up
	vkDestroyFence(device, fence, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);

	for (auto &b : commandBuffer)
	{
		vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(1), &b);
	}

	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyBuffer(device, uniformBuffer, nullptr);
	vkFreeMemory(device, uboMem, nullptr);
}
