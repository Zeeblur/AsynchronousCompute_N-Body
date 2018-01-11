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

	bool isComplete() {
		return graphicsFamily >= 0 && presentFamily >= 0;
	}
};

// struct to pass the swapchain details 
struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};


// structs to store vertex attributes
struct Vertex
{
	glm::vec3 pos;
	glm::vec3 colour;
	glm::vec2 texCoord;

	Vertex(glm::vec3 p, glm::vec3 c, glm::vec2 t) : pos(p), colour(c), texCoord(t){}

	// how to pass to vertex shader
	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription = {};

		// 1 binding as all data is in 1 array. binding is index.
		// stride is bytes between entries (in this case 1 whole vertex struct)
		// move to the next data entry after each vertex (not instanced rendering)
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;


		return bindingDescription;
	}

	// get attribute descriptions...
	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescription()
	{
		// 2 attributes (position and colour) so two description structs
		std::array<VkVertexInputAttributeDescription, 3> attributeDesc = {};

		attributeDesc[0].binding = 0; // which binding (the only one created above)
		attributeDesc[0].location = 0; // which location of the vertex shader
		attributeDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT; // format as a vector 3 (3floats)
		attributeDesc[0].offset = offsetof(Vertex, pos); // calculate the offset within each Vertex

		// as above but for colour
		attributeDesc[1].binding = 0; 
		attributeDesc[1].location = 1;
		attributeDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT; // format as a vector 3 (3floats)
		attributeDesc[1].offset = offsetof(Vertex, colour); 
		 
		// texture layout
		attributeDesc[2].binding = 0;
		attributeDesc[2].location = 2;
		attributeDesc[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDesc[2].offset = offsetof(Vertex, texCoord);

		return attributeDesc;
	}
};

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

enum bufferType
{
	VERTEX,
	INDEX,
	INSTANCE
};

struct BufferObject; // forward declare

// Resources for the compute part of the example
struct ComputeConfig;

class Application
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
	VkCommandPool commandPool;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;


	struct ComputeConfig
	{
		BufferObject* storageBuffer;					// (Shader) storage buffer object containing the particles
		VkBuffer* uniformBuffer;		    // Uniform buffer object containing particle system parameters
		VkQueue queue;								// Separate queue for compute commands (queue family may differ from the one used for graphics)
		VkCommandPool commandPool;					// Use a separate command pool (queue family may differ from the one used for graphics)
		VkCommandBuffer commandBuffer;				// Command buffer storing the dispatch commands and barriers
		VkFence fence;								// Synchronization fence to avoid rewriting compute CB if still in use
		VkDescriptorSetLayout descriptorSetLayout;	// Compute shader binding layout
		VkDescriptorSet descriptorSet;				// Compute shader bindings
		VkPipelineLayout pipelineLayout;			// Layout of the compute pipeline
		VkPipeline pipeline;						// Compute pipeline for updating particle positions

													// Compute shader uniform block object
		struct computeUBO
		{
			float deltaT;							//		Frame delta time
			float destX;							//		x position of the attractor
			float destY;							//		y position of the attractor
			int32_t particleCount = 0;
		} ubo;
	} compute;

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
	void initVulkan();

	void cleanup();
	void cleanupSwapChain();
	 
	void recreateSwapChain();

	static void onWindowResized(GLFWwindow* window, int width, int height)
	{
		if (width == 0 || height == 0) return;

		Application* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		app->recreateSwapChain();
	}

	//Vulkan Instance methods
	void createInstance();
	void setupDebugCallback();
	void createSurface();
	void pickPhysicalDevice();
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
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void createTextureImageView();
	VkImageView Application::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
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

	void drawFrame();
	void updateUniformBuffer();

	// checks
	bool checkValidationLayerSupport();
	std::vector<const char*> Application::getExtensions();
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	bool isDeviceSuitable(VkPhysicalDevice device);
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

public:

	inline static std::shared_ptr<Application> get()
	{
		static std::shared_ptr<Application> instance(new Application());
		return instance;
	}

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuff, VkBuffer targetBuff, VkDeviceSize size);


	void init()
	{
		initWindow();
		initVulkan();
	}

	void mainLoop();

	void setVertexData(const std::vector<Vertex> vert, const std::vector<uint16_t> ind, const std::vector<particle> part);
	void createConfig(int pCount);
	int PARTICLE_COUNT = 0;

	void clean()
	{
		cleanup();
	}
};


struct BufferObject
{
	VkDevice* dev;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	size_t size = 0;

	virtual void createBuffer() = 0;

	~BufferObject()
	{
		vkDestroyBuffer(*dev, buffer, nullptr);
		vkFreeMemory(*dev, memory, nullptr);
	}
};

struct VertexBO : BufferObject
{
	std::vector<Vertex> vertices;
	void createBuffer()
	{
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
		size = (size_t)bufferSize;

		// create a staging buffer as a temp buffer and then the device has a local vertex  buffer.
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		Application::get()->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		// copy data to buffer
		void* data;
		// map buffer memory to the cpu
		vkMapMemory(*dev, stagingBufferMemory, 0, bufferSize, 0, &data);


		// Copy vertex data into the mapped memory
		memcpy(data, vertices.data(), size);

		// unmap buffer
		vkUnmapMemory(*dev, stagingBufferMemory);

		// create vertex buffer
		Application::get()->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);

		// local so can't use map., so have to copy data between buffers.
		Application::get()->copyBuffer(stagingBuffer, buffer, bufferSize);

		// clean up staging buffer
		vkDestroyBuffer(*dev, stagingBuffer, nullptr);
		vkFreeMemory(*dev, stagingBufferMemory, nullptr);
	}
};

struct IndexBO : BufferObject
{
	std::vector<uint16_t> indices;

	void createBuffer()
	{

		// buffersize is the number of incides times the size of the index type (unit32/16)
		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
		size = indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		Application::get()->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(*dev, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t)bufferSize);
		vkUnmapMemory(*dev, stagingBufferMemory);

		// note usage is INDEX buffer. 
		Application::get()->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);

		Application::get()->copyBuffer(stagingBuffer, buffer, bufferSize);
		 
		vkDestroyBuffer(*dev, stagingBuffer, nullptr);
		vkFreeMemory(*dev, stagingBufferMemory, nullptr);
	}
};

struct InstanceBO : BufferObject
{
	std::vector<particle> particles;
	void createBuffer()
	{

		// buffersize is the number of incides times the size of the index type (unit32/16)
		VkDeviceSize bufferSize = sizeof(particles[0]) * particles.size();
		size = particles.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		Application::get()->createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory);

		void* data; 
		vkMapMemory(*dev, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, particles.data(), (size_t)bufferSize);
		vkUnmapMemory(*dev, stagingBufferMemory);

		// note usage is INDEX buffer. 
		Application::get()->createBuffer(bufferSize,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			buffer,
			memory); 

		Application::get()->copyBuffer(stagingBuffer, buffer, bufferSize);

		vkDestroyBuffer(*dev, stagingBuffer, nullptr);
		vkFreeMemory(*dev, stagingBufferMemory, nullptr);
	}

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription vInputBindDescription{};
		vInputBindDescription.binding = 1;   // bind this to 1 (vertex is 0)
		vInputBindDescription.stride = sizeof(particle);
		vInputBindDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
		return vInputBindDescription;
	}

	static VkVertexInputAttributeDescription getAttributeDescription()
	{
		// 1 attributes (position)
		VkVertexInputAttributeDescription attributeDesc;

		attributeDesc.binding = 1; // which binding (the only one created above)
		attributeDesc.location = 3; // which location of the vertex shader
		attributeDesc.format = VK_FORMAT_R32G32B32_SFLOAT; // format as a vector 3 (3floats)
		attributeDesc.offset = 0;// sizeof(float) * 3;// offsetof(particle, pos); // calculate the offset within each Vertex

		return attributeDesc;
	}
};