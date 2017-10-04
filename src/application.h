#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <vector>


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

// struct for queues
struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int surfaceFamily = -1;

	bool isComplete() {
		return graphicsFamily >= 0 && surfaceFamily >= 0;
	}
};

// struct to pass the swapchain details 
struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
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

	void initWindow();
	void initVulkan();
	void mainLoop();
	void cleanup();

	//Vulkan Instance methods
	void createInstance();
	void setupDebugCallback();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();

	// checks
	bool checkValidationLayerSupport();
	std::vector<const char*> Application::getExtensions();
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	bool isDeviceSuitable(VkPhysicalDevice device);

	// find queues
	QueueFamilyIndices findQueuesFamilies(VkPhysicalDevice device);

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

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


public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}
};