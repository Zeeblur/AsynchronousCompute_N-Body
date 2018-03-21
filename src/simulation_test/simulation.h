#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "buffer.h"
#include "compute.h"
#include <memory>

class Renderer;
struct QueueFamilyIndices;

class simulation
{
protected:
	std::shared_ptr<Renderer> renderer;

	// store queues
	const VkQueue& presentQueue;
	const VkQueue& graphicsQueue;
	const VkDevice& device;
	int buffIndex = 0;
	

	virtual ~simulation() = 0;

public:
		
	simulation(const VkQueue* pQ, const VkQueue* gQ, const VkDevice* dev)
		: presentQueue(*pQ), graphicsQueue(*gQ), device(*dev)
	{
		buffers[VERTEX] = new VertexBO();
		buffers[INDEX] = new IndexBO();
		buffers[INSTANCE] = new InstanceBO();

		for (auto &b : buffers)
		{
			b->dev = &device;
		}
	}

	BufferObject* buffers[3];//  { new VertexBO(), new IndexBO(), new InstanceBO(); };

	virtual void frame() = 0;
	virtual void createCommandPools(QueueFamilyIndices& queueFamilyIndices, VkPhysicalDevice& phys);
	virtual void allocateComputeCommandBuffers() = 0;
	virtual void recordComputeCommands() = 0;
	virtual void recordGraphicsCommands();
	virtual void createDescriptorPool();
	virtual void createDescriptorSets();
	virtual void createBufferObjects() = 0; // vertex instance and index buffer data 
	virtual void dispatchCompute() = 0;
	virtual void cleanup() = 0;

	ComputeConfig* compute;
};

class comp_simulation : public simulation
{
private:
	void frame() override;
	void createCommandPools(QueueFamilyIndices& queueFamilyIndices, VkPhysicalDevice& phys) override;
	void allocateComputeCommandBuffers() override;
	void recordComputeCommands() override;
	void createBufferObjects() override;
	void dispatchCompute() override;
	void cleanup() override;


	int findComputeQueueFamily(VkPhysicalDevice device);
public:
	comp_simulation(const VkQueue* pQ, const VkQueue* gQ, const VkDevice* dev);
	
};

class trans_simulation : public simulation
{
private:
	void frame() override;
	void createCommandPools(QueueFamilyIndices& queueFamilyIndices, VkPhysicalDevice& phys) override;
	void allocateComputeCommandBuffers() override;
	void recordComputeCommands() override;
	void createBufferObjects() override;
	void dispatchCompute() override;
	void cleanup() override;

	void copyComputeResults();
	void computeTransfer();
	void recordTransferCommands();

	VkCommandBuffer transferCmdBuffer;
	VkQueue transferQueue;
	VkCommandPool transferPool;
	int findTransferQueueFamily(VkPhysicalDevice pd);
public:
	trans_simulation(const VkQueue* pQ, const VkQueue* gQ, const VkDevice* dev);
};

class double_simulation : public simulation
{
	void frame() override;
	void allocateComputeCommandBuffers() override;
	void recordComputeCommands() override;
	void recordGraphicsCommands() override;
	void createBufferObjects() override;
	void dispatchCompute() override;
	void cleanup() override;

	void recordRenderCommand(int frame);
	void recordComputeCommand(int frame);
	void allocateGraphicsCommandBuffers();
	void createDescriptorPool();
	void createDescriptorSets();
public:
	double_simulation(const VkQueue* pQ, const VkQueue* gQ, const VkDevice* dev);
	int bufferIndex = 0;
	void waitOnFence(VkFence& fence);
};