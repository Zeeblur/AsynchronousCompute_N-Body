#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "buffer.h"
#include "compute.h"
#include <memory>

class Renderer;

class simulation
{
protected:
	std::shared_ptr<Renderer> renderer;

	virtual void cleanup() = 0;

	// recording the command buffers?

	// store queues
	const VkQueue& presentQueue;
	const VkQueue& graphicsQueue;
	const VkDevice& device;

	

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
	virtual void allocateCommandBuffers() = 0;
	virtual void recordComputeCommands() = 0;
	virtual void createBufferObjects() = 0; // vertex instance and index buffer data 

	ComputeConfig* compute;
};

class comp_simulation : public simulation
{
private:
	void frame() override;
	void allocateCommandBuffers() override;
	void recordComputeCommands() override;
	void createBufferObjects() override;
	void cleanup() override;

	void dispatchCompute();
public:
	comp_simulation(const VkQueue* pQ, const VkQueue* gQ, const VkDevice* dev);
	
};

class trans_simulation : public simulation
{
private:
	void frame() override;
	void allocateCommandBuffers() override;
	void recordComputeCommands() override;
	void createBufferObjects() override;
	void cleanup() override;

	void copyComputeResults();
	void computeTransfer();
	void recordTransferCommands();

	VkCommandBuffer transferCmdBuffer;
public:
	trans_simulation(const VkQueue* pQ, const VkQueue* gQ, const VkDevice* dev);
};

class double_simulation : public simulation
{
	void frame() override;
	void allocateCommandBuffers() override;
	void recordComputeCommands() override;
	void createBufferObjects() override;
	void cleanup() override;
public:
	double_simulation(const VkQueue* pQ, const VkQueue* gQ, const VkDevice* dev);
};