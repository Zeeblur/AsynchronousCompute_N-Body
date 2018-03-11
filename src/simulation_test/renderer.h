#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
#include <array>
#include <memory>
#include "particle.h"
#include "buffer.h"
#include <chrono>
#include "simulation.h"
#include "compute.h"

using namespace std::chrono;

struct Vertex;
enum MODE;

// if debugging - do INSTANCE validation layers
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// read in binaries/ shader SPIR-V files
static std::vector<char> readFile(const std::string& filename) 
{
	// ate - start reading at the end of file to allocate a buffer for the size of the text
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("cannot open file! " + filename);

	// allocate size
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	// start at begining and fill buffer up to stream count (file size)
	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

// struct for queues
struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentFamily = -1;
	int computeFamily = -1;

	// check if families are complete. gfx compute present
	bool isComplete() {
		return graphicsFamily >= 0 && presentFamily >= 0 && computeFamily >= 0;
	}
};

// struct to pass the swapchain details 
struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct BufferObject; // forward declare

// Resources for the compute part of the example
struct ComputeConfig;

class Renderer
{
private:
	GLFWwindow* window;
	VkInstance instance;
	VkDebugReportCallbackEXT callback;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	VkRenderPass renderPass;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	VkCommandPool gfxCommandPool;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;

	time_point<system_clock> currentTime;
	VkPipelineCache pipeCache;

	void createComputeUBO();

	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	std::vector<VkCommandBuffer> commandBuffers;

	// to hold the indicies of the queue families
	struct
	{
		uint32_t graphics;
		uint32_t compute;
		uint32_t present;
	} queueFamilyIndices;


	void initWindow();
	void initVulkan(const bool AMD);

	void cleanup();
	void cleanupSwapChain();
	 
	void recreateSwapChain();

	static void onWindowResized(GLFWwindow* window, int width, int height)
	{
		if (width == 0 || height == 0) return;

		Renderer* app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
		app->recreateSwapChain();
	}

	//Vulkan Instance methods
	void createInstance();
	void setupDebugCallback();
	void createSurface();
	void pickPhysicalDevice(const bool AMD);
	void createLogicalDevice();
	void createSwapChain();
	void createImageViews();
	void createRenderPass();
	void createDescriptorSetLayout();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createInstanceBuffer();

	// texture stuff
	void createTextureImage();
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void createTextureImageView();
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	void createTextureSampler();

	// depth buffer
	void createDepthResources();
	VkFormat findDepthFormat();
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	
	BufferObject* buffers[3];//  { new VertexBO(), new IndexBO(), new InstanceBO(); };

	void createVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffer();
	void createDescriptorPool();
	void createDescriptorSet();

	void createCommandBuffers();
	void buildComputeCommandBuffer();
	void createSemaphores();

	// checks
	bool checkValidationLayerSupport();
	std::vector<const char*> Renderer::getExtensions();
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	bool isDeviceSuitable(VkPhysicalDevice device, const bool AMD);
	bool hasStencilComponent(VkFormat format);

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	// find queues
	QueueFamilyIndices findQueuesFamilies(VkPhysicalDevice device);
	int findComputeQueueFamily(VkPhysicalDevice device);

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkShaderModule createShaderModule(const std::vector<char>& code);

	// create reportcall
	VkResult CreateDebugReportCallbackEXT(
		VkInstance instance,
		const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugReportCallbackEXT* pCallback);

	// call back from validation layers to program
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData);

	// cleanup mem
	void DestroyDebugReportCallbackEXT(VkInstance instance,
		VkDebugReportCallbackEXT callback,
		const VkAllocationCallbacks* pAllocator);

	const int WIDTH = 800;
	const int HEIGHT = 600;

	void prepareCompute();

	// timer vars
	uint32_t frameCounter, lastFPS;
	float frameTimer = 0;
	float fpsTimer = 0;

public:

	simulation* simulation;
	ComputeConfig* compute;

	inline static std::shared_ptr<Renderer> get()
	{
		static std::shared_ptr<Renderer> instance(new Renderer());
		return instance;
	}

	void init(const bool AMD)
	{
		initWindow();
		initVulkan(AMD);
	}

	void mainLoop();

	void setVertexData(const std::vector<Vertex> vert, const std::vector<uint16_t> ind, const std::vector<particle> part);
	void createConfig(const MODE chosenMode, const int pCount);
	int PARTICLE_COUNT = 0;

	// buffer creation & copy functions
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuff, VkBuffer targetBuff, VkDeviceSize size);

	// command queue for recording & submitting copies
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	void drawFrame();
	void updateCompute();
	void updateUniformBuffer();

	void clean()
	{
		cleanup();
	}
};
