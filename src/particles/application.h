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
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
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

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

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
	

	// TODO: SHOULDN'T ALLOCATE MEMORY FOR EVERY OBJECT INDIVIDUALLY - NEED TO IMPLEMENT ALLOCATOR
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuff, VkBuffer targetBuff, VkDeviceSize size);
	void createVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffer(); // NOT MOST EFFICIENT WAY TODO: CHANGE TO PUSH CONSTANTS
	void createDescriptorPool();
	void createDescriptorSet();

	void createCommandBuffers();
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

public:

	inline static std::shared_ptr<Application> get()
	{
		static std::shared_ptr<Application> instance(new Application());
		return instance;
	}

	void init()
	{
		initWindow();
		initVulkan();
	}

	void mainLoop();

	void setVertexData(const std::vector<Vertex>& vert, const std::vector<uint16_t>& ind);
	void createConfig();

	void clean()
	{
		cleanup();
	}
};