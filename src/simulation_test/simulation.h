#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "buffer.h"
#include "compute.h"

class Renderer;

class simulation
{
protected:
	Renderer* renderer;
	ComputeConfig* compute;

	// command buffers();
	virtual void createCommandBuffers() = 0;
	virtual void cleanup() = 0;

	// recording the command buffers?

	// store queues
	const VkQueue& presentQueue;
	const VkQueue& graphicsQueue;
	const VkDevice& device;

	simulation(const VkQueue* pQ, const VkQueue* gQ, const VkDevice* dev)
		: presentQueue(*pQ), graphicsQueue(*gQ), device(*dev)
	{	}

	virtual ~simulation() = 0;

public:
		
	virtual void frame() = 0;

};

class comp_simulation : simulation
{
private:
	void frame() override;
	void createCommandBuffers() override;
	void cleanup() override;

	void dispatchCompute();
public:
	comp_simulation(const VkQueue* pQ, const VkQueue* gQ, const VkDevice* dev);
};

class trans_simulation : simulation
{
private:
	void frame() override;
	void createCommandBuffers() override;
	void cleanup() override;

	void copyComputeResults();
	void computeTransfer();

	VkCommandBuffer transferCmdBuffer;
public:
	trans_simulation(const VkQueue* pQ, const VkQueue* gQ, const VkDevice* dev);
};

//class double_simulation : simulation
//{
//	void frame() override;
//	void createCommandBuffers() override;
//	void cleanup() override;
//public:
//	double_simulation(const VkQueue* pQ, const VkQueue* gQ, const VkDevice* dev);
//};