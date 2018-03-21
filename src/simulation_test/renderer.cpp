#include "renderer.h"
#include "nbody.h"
#include <set>
#include <chrono>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fstream>
#include <sstream>

// Custom define for better code readability
#define VK_FLAGS_NONE 0

void Renderer::initWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);

	// create call back gfor resize
	glfwSetWindowUserPointer(window, this);
	glfwSetWindowSizeCallback(window, Renderer::onWindowResized);
} 

void Renderer::initVulkan(const MODE chosenMode, const bool AMD)
{
	// store mode
	chosenSimMode = chosenMode;

	switch (chosenMode)
	{
	case COMPUTE:
		sim = new comp_simulation(&presentQueue, &graphicsQueue, &device);
		break;
	case TRANSFER:
		sim = new trans_simulation(&presentQueue, &graphicsQueue, &device);
		break;
	case DOUBLE:
		sim = new double_simulation(&presentQueue, &graphicsQueue, &device);
		break;
	}

	// set compute config
	compute = sim->compute;

	createInstance();
	setupDebugCallback();
	createSurface();
	pickPhysicalDevice(AMD);
	createLogicalDevice();
	createSwapChain();
}

void Renderer::createConfig(const parameters& simParam)
{
	simulationParameters = &simParam;
	PARTICLE_COUNT = simParam.pCount;
	lighting = simParam.lighting;

	createImageViews();
	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createCommandPool();
	createDepthResources();
	createFramebuffers();
	createTextureImage();
	createTextureImageView();
	createTextureSampler();

	sim->createBufferObjects();

	createUniformBuffer();
	sim->createDescriptorPool();
	createDescriptorSet();

	createQueryPools();

	sim->recordGraphicsCommands();
	createSemaphores();

	prepareCompute();
}

enum STAGES
{
	G_START = 0,
	G_END = 2,
	C_START = 4,
	C_END = 6
};

bool does_file_exist(std::string fileName)
{
	std::ifstream infile(fileName);
	return infile.good();
}

std::string Renderer::createFileString(int testNum)
{
	std::stringstream filetoSave;
	std::string gpuType = (amdGPU) ? "AMD" : "NVIDIA";


	filetoSave << gpuType << "_S" << (int)chosenSimMode << "_P" << PARTICLE_COUNT <<
		"_ST" << simulationParameters->stacks <<
		"_SL" << simulationParameters->slices <<
		"_SC" << simulationParameters->dims.x <<
		"_TN" << testNum <<
		".csv";

	return filetoSave.str();
}

void Renderer::mainLoop()
{
	int testNumber = 0;
	std::string fileName = createFileString(testNumber);
	
	// check if file exists.. if so increment number.
	while (does_file_exist(fileName))
	{
		testNumber++;
		fileName = createFileString(testNumber);
	}

	// store in file
	std::ofstream file(fileName, std::ofstream::out);

	// header
	file << "Simulation Type" << ", " << simulationParameters->modeTypes[chosenSimMode] << std::endl;
	file << "Particles, " << PARTICLE_COUNT << ", "
		<< "Stack Count, " << simulationParameters->stacks << ", "
		<< "Slice Count, " << simulationParameters->slices << ", "
		<< "Mesh Scale, " << simulationParameters->dims.x  // assuming only square scales.
		<< std::endl;

	file << "Frame" << ", "
		<< "Frame Time (ms)" << ", "
		<< "Compute Timestamp Start" << ", "
		<< "Compute Timestamp End" << ", "
		<< "Compute Time" << ", "
		<< "Graphics Timestamp Start" << ", "
		<< "Graphics Timestamp End" << ", "
		<< "Graphics Time" << ", " 
		<< "async?" << std::endl;


	while (!glfwWindowShouldClose(window))
	{

		if (secondsRan > simulationParameters->totalTime)
		{
			exit(0);
		}

		// start timer
		auto startTime = std::chrono::high_resolution_clock::now();

		sim->frame();

		frameCounter++;
		auto endTime = std::chrono::high_resolution_clock::now();
		auto deltaT = std::chrono::duration<double, std::milli>(endTime - startTime).count();
		frameTimer = (float)deltaT / 1000.0f;

		// reset comput now to get results back
		if (chosenSimMode == DOUBLE)
		{
			dynamic_cast<double_simulation*>(sim)->waitOnFence(compute->fence);
		}


		fpsTimer += (float)deltaT;
		if (fpsTimer > 1000.0f)  // after 1 second
		{
			lastFPS = static_cast<uint32_t>(1.0f / frameTimer);

			std::cout << std::fixed << deltaT << "ms (" << lastFPS << " fps)" << std::endl;

			fpsTimer = 0.0f;
			secondsRan++;
		}

		// Fetch results.
		std::uint64_t results[8] = {};
		vkGetQueryPoolResults(device, renderQueryPool, 0, 1, sizeof(std::uint64_t) * 2, &results[G_START], 0, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);  // graphics start
		vkGetQueryPoolResults(device, renderQueryPool, 1, 1, sizeof(std::uint64_t) * 2, &results[G_END], 0, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);	 // graphics end
		vkGetQueryPoolResults(device, computeQueryPool, 0, 1, sizeof(std::uint64_t) * 2, &results[C_START], 0, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT); // compute start
		vkGetQueryPoolResults(device, computeQueryPool, 1, 1, sizeof(std::uint64_t) * 2, &results[C_END], 0, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT); // compute end

		std::uint64_t initial = 0;
		bool async = false;

		// graphics starts execution first
		if (results[G_START] < results[C_START])
		{
			//std::cout << "Gfx" << std::endl;
			// if gfx is first. Async if start of c overlaps g end,
			if (results[C_START] < results[G_END])
			{
				//std::cout << "ASYNCASYNCASYNC" << std::endl;
				async = true;
			}
		}
		else
		{
			//std::cout << "Compute" << std::endl;
			// if compute is first. Async if start of g overlaps c end,
			if (results[G_START] < results[C_END])
			{
				//std::cout << "ASYNCASYNCASYNC" << std::endl;
				async = true;
			}
		}

		glfwPollEvents();

		// print results to file

		file << frameCounter << ", " 
			<< deltaT << ", "
			<< results[C_START] << ", "
			<< results[C_END] << ", "
			<< (results[C_END] - results[C_START]) * timestampPeriod / 1000000.0 << ", "
			<< results[G_START] << ", "
			<< results[G_END] << ", "
			<< (results[G_END] - results[G_START]) * timestampPeriod / 1000000.0 << ", "
			<< (async ? "YES" : "NO") << std::endl;
	}

	vkDeviceWaitIdle(device);

}
 
void Renderer::cleanupSwapChain()
{
	vkDestroyImageView(device, depthImageView, nullptr);
	vkDestroyImage(device, depthImage, nullptr);
	vkFreeMemory(device, depthImageMemory, nullptr);

	for (size_t i = 0; i < swapChainFramebuffers.size(); i++)
	{
		vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
	}

	vkFreeCommandBuffers(device, gfxCommandPool, static_cast<uint32_t>(graphicsCmdBuffers.size()), graphicsCmdBuffers.data());

	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);

	for (size_t i = 0; i < swapChainImageViews.size(); i++)
	{
		vkDestroyImageView(device, swapChainImageViews[i], nullptr); 
	}

	vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void Renderer::cleanup()
{
	cleanupSwapChain();

	vkDestroySampler(device, textureSampler, nullptr);
	vkDestroyImageView(device, textureImageView, nullptr);

	vkDestroyImage(device, textureImage, nullptr);
	vkFreeMemory(device, textureImageMemory, nullptr);

	if (lighting)
	{
		vkDestroySampler(device, textureSampler_Normal, nullptr);
		vkDestroyImageView(device, textureImageView_Normal, nullptr);

		vkDestroyImage(device, textureImage_Normal, nullptr);
		vkFreeMemory(device, textureImageMemory_Normal, nullptr);
	}

	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	vkDestroyBuffer(device, uniformBuffer, nullptr);
	vkFreeMemory(device, uniformBufferMemory, nullptr);


	// rememebr to call cleanup on compute
	sim->cleanup();

	// destroy buffers
	for (auto &b : sim->buffers)
	{
		delete b;
	}
	
	vkDestroyQueryPool(device, renderQueryPool, nullptr);
	vkDestroyQueryPool(device, computeQueryPool, nullptr);
	vkDestroyFence(device, graphicsFence, nullptr);

	vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

	if (chosenSimMode != DOUBLE)
		vkDestroyCommandPool(device, gfxCommandPool, nullptr);

	vkDestroyDevice(device, nullptr);
	DestroyDebugReportCallbackEXT(instance, callback, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);

	glfwTerminate();
}

void Renderer::recreateSwapChain()
{
	vkDeviceWaitIdle(device);

	cleanupSwapChain();

	createSwapChain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createDepthResources();
	createFramebuffers();
	sim->recordGraphicsCommands();
}

// create a struct with driver specific details
void Renderer::createInstance()
{
	// check if validation layers exist
	if (enableValidationLayers && !checkValidationLayerSupport())
		throw std::runtime_error("validation layers requested, but not available!");


	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "High Performance Graphics";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	// to tell the driver which global ext and validation layers to use.
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// create extensions and set info
	auto extensions = getExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();


	// get/set validation layers if flag true
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	// try creating instance
	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
		throw std::runtime_error("Failed to create vulkan instance");

}

// set up callbacks if needed
void Renderer::setupDebugCallback()
{
	// not enabled so don't initialise
	if (!enableValidationLayers) return;

	// set the info
	VkDebugReportCallbackCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	createInfo.pfnCallback = debugCallback;

	// create the callback
	if (CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS)
		throw std::runtime_error("failed to set up debug callback!");
	

}

// create surface for interfacing with GLFW to render things on screen
void Renderer::createSurface()
{
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
		throw std::runtime_error("Failed to create glfw surface.");
}

// select GPU that supports the features needed
void Renderer::pickPhysicalDevice(const bool AMD)
{
	// find devices and store in vector
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);


	// create vector and store
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	if (deviceCount == 0)
		throw std::runtime_error("Failed to find GPU with Vulkan support!");

	for (const auto& dev : devices)
	{
		std::cout << "checking device" << std::endl;

		if (isDeviceSuitable(dev, AMD)) 
		{
			physicalDevice = dev;
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE)
		throw std::runtime_error("failed to find a suitable GPU!");
	
	std::cout << "device suitable" << std::endl;
}

// create a logic device + queues
void Renderer::createLogicalDevice()
{
	// specify the queues to create
	// returns indicies of queues that can support graphics calls
	QueueFamilyIndices indices = findQueuesFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQFamilies = { indices.graphicsFamily, indices.presentFamily, indices.computeFamily };
	
	queueFamilyIndices.graphics = indices.graphicsFamily;
	queueFamilyIndices.compute = indices.computeFamily;
	queueFamilyIndices.present = indices.presentFamily;

	// influence sheduling of the command buffer (NEEDED EVEN IF ONLY 1 Q)
	float queuePriority[2] = { 1.0f, 1.0f };

	// for each unique queue family, create a new info for them and add to list
	for (int queueFam : uniqueQFamilies)
	{
		VkDeviceQueueCreateInfo qCreateInfo;
		qCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		
		// need to ensure these are null so no garbage
		qCreateInfo.flags = 0;
		qCreateInfo.pNext = NULL;

		// this is either graphics/surface etc
		qCreateInfo.queueFamilyIndex = queueFam;

		qCreateInfo.pQueuePriorities = queuePriority;
		qCreateInfo.queueCount = (queueFam == indices.graphicsFamily && queueFam == indices.computeFamily) ? 2 : 1;

		// add to list
		queueCreateInfos.push_back(qCreateInfo);
	}
	
	// define features wanted to use **
	VkPhysicalDeviceFeatures deviceFeatures = {};

	// create info for the logical device
	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	// add queue info and devices
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;

	// swap chain extensions enable!
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// create validation
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	// instantiate object including the phys device to interface to.
	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
		throw std::runtime_error("Failed to create logical device!");

	vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily, (indices.graphicsFamily == indices.computeFamily) ? 1 : 0, &presentQueue);
	vkGetDeviceQueue(device, indices.computeFamily, 0, &compute->queue);
	std::cout << "Graphics queue: " << graphicsQueue << std::endl;
	std::cout << "Compute queue: " << compute->queue << std::endl;
	
}

// create swapchain
void Renderer::createSwapChain()
{
	// get capabilities
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

	// choose from valid swapchain modes
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	// set the amount of images in the swap chain (queue length)
	// try to get the min needed to render properly and have 1 more than that for triple buffering
	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

	// if it's not zero and higher than the max... change it to max
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	// set image count to 2 for double buffering
	if (chosenSimMode == DOUBLE)
	{
		imageCount = 2;
	}

	// create info for swapchain
	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	// which surface to tie to
	createInfo.surface = surface;

	// details chosen
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;

	// image layers always 1 unless stereoscopic image
	createInfo.imageArrayLayers = 1;

	// transfer to colour - not to a texture - no post-processing for now.
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;


	// specify how to handle images from different queues.
	QueueFamilyIndices indices = findQueuesFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };

	// specify how the data is shared bewteen multiple queues
	if (indices.graphicsFamily != indices.presentFamily)
	{
		// no explicit ownership transfers - images are shared.
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;

		// specify which queue families own/share this.
		createInfo.queueFamilyIndexCount = sizeof(queueFamilyIndices) / sizeof(queueFamilyIndices[0]);
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else // if the graphics and present family are the same - stick to exclusive.
	{
		// An image is owned by one queue family at a time and ownership must be explicitly transfered before using it in another queue family.
		//This option offers the best performance.
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	// can transform image if needed.
	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

	// alpha blending layers -- opaque ignores this
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	// doesn't care about pixels that are obsured by something
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;



	// null for now - but this stores old chain for when it needs to be re-created entirely due to a window resize or something.
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	// create the damn thing!  -- the logical device, swap chain creation info, optional custom allocators and a pointer to the variable to store the handle in
	if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swap chain!");
	}

	// retrieve handles to swapchain imgs
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

}

// create image views for the swapchain images so they can be used in the render pipeline.
void Renderer::createImageViews()
{
	// resize list to store all created images
	swapChainImageViews.resize(swapChainImages.size());

	// iterate through images
	for (size_t i = 0; i < swapChainImages.size(); ++i)
	{
		// As normal, createinfo for the object
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];

		// specify how to treat images 1d/2d/3d etc
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChainImageFormat;

		// default colour mapping
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		// what is the image's purpose - which part should be accessed (for this just colour targets for rendering) - no mipmaps or layers
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		// create the view and store in the list.
		if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create image views!");
		
	}
}

void Renderer::createQueryPools()
{
	VkQueryPoolCreateInfo queryPoolInfo = {};
	queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	queryPoolInfo.queryCount = 2;

	if (vkCreateQueryPool(device, &queryPoolInfo, nullptr, &renderQueryPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create render query pool.");

	if (vkCreateQueryPool(device, &queryPoolInfo, nullptr, &computeQueryPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create compute query pool.");
	
}


// render pass - specifies buffers and samples 
void Renderer::createRenderPass()
{
	// create a single colour buffer represented by a swap chain image
	VkAttachmentDescription colourAttachment = {};
	colourAttachment.format = swapChainImageFormat;
	colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // not multisampling 
	
	// clear buffer at the start
	colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// store colour buffer memory to be able to access later
	colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	// if using a stencil buffer
	colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// don't care what the initial image was layed out. but after render pass make it ready for presentation
	colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// dpeth attachment
	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = findDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Subpasses and attachments.
	// 1 render pass can be multiple subpasses (i.e post processing)
	VkAttachmentReference colourAttachmentRef = {};
	colourAttachmentRef.attachment = 0;
	colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// just do 1 subpass for now specify the colour attachgment
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colourAttachmentRef;   // INDEX OF THIS IS THE LAYOUT LOCATION IN THE SHADER
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	// create subpass dependencies 
	// specify operations to wait on and where they occur. - need to wait for the swap chain to finish reading before accessing it
	// so wait for the colour attachment stage
	// this prevents the render pass from transistioning before we need to start writing colours to it

	// array of depthbuffer
	std::array<VkAttachmentDescription, 2> attachments = { colourAttachment, depthAttachment };

	//// CREATE RENDER PASS

	// Subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
		throw std::runtime_error("Failed to create Render Pass!");


}

void Renderer::createDescriptorSetLayout()
{
	// bind through a layoutstruct  -- set as uniform buffer
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1; // can have an array (different MVP for each animation etc)
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // uniform for vertex
	uboLayoutBinding.pImmutableSamplers = nullptr; // for image sampling

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::vector<VkDescriptorSetLayoutBinding> bindings;

	// create info + create!  // default two 
	bindings.push_back(uboLayoutBinding);
	bindings.push_back(samplerLayoutBinding);

	// if lighting need 2nd texture sampler
	if (lighting)
	{
		VkDescriptorSetLayoutBinding samplerLayoutBindingNorm = {};
		samplerLayoutBindingNorm.binding = 2;
		samplerLayoutBindingNorm.descriptorCount = 1;
		samplerLayoutBindingNorm.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBindingNorm.pImmutableSamplers = nullptr;
		samplerLayoutBindingNorm.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings.push_back(samplerLayoutBindingNorm);
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptior set layout!");
}

// these need to be recreated with any shader change etc.
void Renderer::createGraphicsPipeline()
{
	std::string vert = "vert";
	std::string frag = "frag";

	// read in shader files.
	if (lighting)
	{
		// use phong shaders
		vert += "_phong";
		frag += "_phong";
	}

	auto vertShaderCode = readFile("res/shaders/" + vert + ".spv");
	auto fragShaderCode = readFile("res/shaders/" + frag + ".spv");

	// modules only needed in creation of pipeline so can be destroyed locally
	VkShaderModule vertShaderMod;
	VkShaderModule fragShaderMod; 

	vertShaderMod = createShaderModule(vertShaderCode);
	fragShaderMod = createShaderModule(fragShaderCode);

	// assign the shaders
	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderMod;
	vertShaderStageInfo.pName = "main";		// function to invoke

	// can use here specialisation info to specify shader constant values

	// assign the fragment
	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderMod;
	fragShaderStageInfo.pName = "main";		// function to invoke

	// define array for these structs
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// define fixed functions - explict about data bindings and attributes
	std::vector<VkVertexInputBindingDescription> bindingDesc;
	std::vector<VkVertexInputAttributeDescription> attributeDesc;

	// get binding and attribute descriptions  
	bindingDesc.push_back(Vertex::getBindingDescription());	     // binding for vertex
	bindingDesc.push_back(InstanceBO::getBindingDescription());  // binding for instance

	for (auto &d : Vertex::getAttributeDescription())
	{
		attributeDesc.push_back(d);
	} 

	// push back instance attributes
	for (auto &d : InstanceBO::getAttributeDescription())
	{
		attributeDesc.push_back(d);
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	// where is it bound
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDesc.size());
	vertexInputInfo.pVertexBindingDescriptions = bindingDesc.data();

	// size of att desc
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDesc.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDesc.data(); 

	// specify the geometry to draw fdrom the vertices can use topology or element buffers here
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// specify the viewport - region of frame buffer to output to - usually 0 - width
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// scissor rectangle is like a filter - region in where to store the pixels 
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;  // cover the entire viewport for now

	// create a viewport from info
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	// rasterizer (can chose wireframe/face culling etc here)
	VkPipelineRasterizationStateCreateInfo rasteriser = {};
	rasteriser.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasteriser.depthClampEnable = VK_FALSE;   // clamp frags near planes. useful in shadowmapping

	// discard enable - geom never gets to this srafe and doesn't draw to framebuffer
	rasteriser.rasterizerDiscardEnable = VK_FALSE;

	// polygon mode - fill or wireframe etc
	rasteriser.polygonMode = VK_POLYGON_MODE_FILL;
	rasteriser.lineWidth = 1.0f;

	// cull faces and vertex winding order
	rasteriser.cullMode = VK_CULL_MODE_BACK_BIT;
	rasteriser.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	// can use this for shadows - alter depth biasing
	rasteriser.depthBiasEnable = VK_FALSE;
	rasteriser.depthBiasConstantFactor = 0.0f; // Optional
	rasteriser.depthBiasClamp = 0.0f; // Optional
	rasteriser.depthBiasSlopeFactor = 0.0f; // Optional


	// anti-aliasing setup - currently set to false
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	// set up depth buffer and stenciling if needed here

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	// colour blending modes

	// blend per frame buffer -- CURRENTLY UNMODIFIED
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	// blending for global settings
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional


	// dynamic states - can change some values without recreating the pipeline
	VkDynamicState dynamicStates[] =
	{
		VK_DYNAMIC_STATE_VIEWPORT,		// able to resize window
		VK_DYNAMIC_STATE_LINE_WIDTH		// able to change line width
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;


	// Set-up pipeline layout for uniform locations
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1; // layout for uniforms
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = 0; // Optional

	// create pipeline
	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2; // vert & frag
	pipelineInfo.pStages = shaderStages;

	// reference the arrays of the create info structs
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasteriser;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional

	// ref fixed function and render pass
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0; // index of subpass where pipeline is to be used.

	// can derive pipelines from existing ones.
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	
	// create the pipeline!
	if (vkCreateGraphicsPipelines(device, pipeCache, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
		throw std::runtime_error("Couldn't create graphics pipeline!");


	// cleanup
	vkDestroyShaderModule(device, fragShaderMod, nullptr);
	vkDestroyShaderModule(device, vertShaderMod, nullptr);

}

// create frame buffers for the images in the swap chain
void Renderer::createFramebuffers()
{
	// resize list for size of swap chain images
	swapChainFramebuffers.resize(swapChainImageViews.size());

	// iterate through all images and create a frame buffer from each
	for (size_t i = 0; i < swapChainImageViews.size(); ++i)
	{
		std::array<VkImageView, 2> attachments = { swapChainImageViews[i], depthImageView };

		VkFramebufferCreateInfo frameBufferInfo = {};
		frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferInfo.renderPass = renderPass;
		frameBufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		frameBufferInfo.pAttachments = attachments.data();
		frameBufferInfo.width = swapChainExtent.width;
		frameBufferInfo.height = swapChainExtent.height;
		frameBufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &frameBufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create frame buffer!");

	}

}

// record operations to perform (like drawing and memory transfers) in command buffer objects
// command pool stores these buffers 
void Renderer::createCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = findQueuesFamilies(physicalDevice);

	sim->createCommandPools(queueFamilyIndices, physicalDevice);
}

// handle layout condistions
void Renderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (hasStencilComponent(format))
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		
	}
	else
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier
	(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	endSingleTimeCommands(commandBuffer);
}


void Renderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(commandBuffer);
}

// depth buffering - need image, imageview and layout
void Renderer::createDepthResources()
{
	VkFormat depthFormat = findDepthFormat();

	createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
	depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

VkFormat Renderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

VkFormat Renderer::findDepthFormat()
{
	return findSupportedFormat
	(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

bool Renderer::hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

// create uniform Buffer
void Renderer::createUniformBuffer()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffer, uniformBufferMemory);
}

// allocate the descriptors. - pool to allocate from, how many to allocate, and the layout to base them on
void Renderer::createDescriptorSet()
{
	VkDescriptorSetLayout layouts[] = { descriptorSetLayout };
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = layouts;

	// allocate
	if (vkAllocateDescriptorSets(device, &allocInfo, &gfxDescriptorSet) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate descriptor set!");

	// auto freed when pool is destroyed.
	// configure which buffer and region of buffer
	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = uniformBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(UniformBufferObject);

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = textureImageView;
	imageInfo.sampler = textureSampler;

	std::vector<VkWriteDescriptorSet> descriptorWrites = {};

	if (lighting)
	{
		descriptorWrites.resize(3);

		VkDescriptorImageInfo* imageInfo2 =  new VkDescriptorImageInfo();
		imageInfo2->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo2->imageView = textureImageView_Normal;
		imageInfo2->sampler = textureSampler_Normal;

		descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[2].dstSet = gfxDescriptorSet;
		descriptorWrites[2].dstBinding = 2;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pImageInfo = imageInfo2;
	}
	else
	{
		descriptorWrites.resize(2);
	}

	// descriptions are updated using UpdateSets... Takes in a write set
	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = gfxDescriptorSet; // which to update
	descriptorWrites[0].dstBinding = 0;			// binding (in the shader layout)
	descriptorWrites[0].dstArrayElement = 0;	// can be arrays so specify index
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;   	// specify type.
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pBufferInfo = &bufferInfo;

	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = gfxDescriptorSet;
	descriptorWrites[1].dstBinding = 1;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[1].descriptorCount = 1;
	descriptorWrites[1].pImageInfo = &imageInfo;
	
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

// create semaphores for rendering synchronisation
void Renderer::createSemaphores()
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// 2 semaphores for if the image is ready (from the swapchain) and iuf the render is finished from the command buffer
	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}
}

// get image from swapchain, execute command buffer with that image in the framebuffer, return the image to the swap chain for presentation
void Renderer::drawFrame()
{
	// asynchronous calls so need to use semaphores/fences

	// 1.  get image from swapchain
	uint32_t imageIndex;
	// logical device, swapchain, timeout (max here), signaled when engine is finished using the image, output when it's become available.
	VkResult result = vkAcquireNextImageKHR(device, swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		throw std::runtime_error("failed to acquire swap chain image!");

	// 2. Submitting the command buffer
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	// wait for if the image is avalible from the swapchain and stored in imageIndex
	VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };

	// Wait at the colour stage of the pipeline - theoretically can implement the vertex shader whilst the image is not ready
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	// which command buffers to submit for exe - the one that binds the swap chain image we aquired as a colour attachment
	submitInfo.commandBufferCount = 1;

	if (chosenSimMode == DOUBLE)
	{
		submitInfo.pCommandBuffers = &graphicsCmdBuffers[dynamic_cast<double_simulation*>(sim)->bufferIndex];
	}
	else
	{
		submitInfo.pCommandBuffers = &graphicsCmdBuffers[imageIndex];
	}

	// which semaphores to signal once the command buffers have finished execution.
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	// submit to queue with signal info. // last param is a fence but we're using semaphores
	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, graphicsFence) != VK_SUCCESS)
		throw std::runtime_error("failed to submit draw command buffer!");
	

	// should return true when render is finished

	// 3. submit the result back to the swap chain
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	// wait for the render to have finished before starting
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	// specify which swapchain to present the image to
	VkSwapchainKHR swapChains[] = { swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	// can specify array of results to check if presentation is successful, but not needed if only 1 swapchain 
	presentInfo.pResults = nullptr; // Optional

	result = vkQueuePresentKHR(presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to present swap chain image!");
	}

}



void Renderer::createTextureImage()
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load("res/textures/Tiles_010_COLOR.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels)
		throw std::runtime_error("failed to load texture image!");
	
	// buffer to copy pixels
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(device, stagingBufferMemory);
	stbi_image_free(pixels);

	createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);

	// create Normal Map
	if (lighting)
	{
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load("res/textures/Tiles_010_NORM.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize = texWidth * texHeight * 4;

		if (!pixels)
			throw std::runtime_error("failed to load texture image!");

		// buffer to copy pixels
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(device, stagingBufferMemory);
		stbi_image_free(pixels);

		createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage_Normal, textureImageMemory_Normal);

		transitionImageLayout(textureImage_Normal, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyBufferToImage(stagingBuffer, textureImage_Normal, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		transitionImageLayout(textureImage_Normal, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}
}

void Renderer::createTextureImageView()
{
	textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

	if (lighting)
	{
		textureImageView_Normal = createImageView(textureImage_Normal, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void Renderer::createTextureSampler()
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 1;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}

	if (lighting)
	{
		if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler_Normal) != VK_SUCCESS)
			throw std::runtime_error("failed to create texture sampler!");
	}
}

VkImageView Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture image view!");
	}

	return imageView;
}

void Renderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(device, image, imageMemory, 0);
}

void Renderer::updateUniformBuffer()
{ 
	static auto startTime = std::chrono::high_resolution_clock::now();  

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

	UniformBufferObject ubo = {}; 
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, -20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10000.0f);

	void* data;  
	vkMapMemory(device, uniformBufferMemory, 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(device, uniformBufferMemory);
}

// wrapper to pass the shader code to the pipeline
VkShaderModule Renderer::createShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();

	// specify a pointer to the buffer with the code and the length of it. need to cast as it's in bytecode size
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	// create the module
	VkShaderModule shaderModule;

	// check it's created
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		throw std::runtime_error("Failed to create shader module!");

	return shaderModule;
}

// check if validation layers are supported by the gpu
bool Renderer::checkValidationLayerSupport()
{
	// find layers.
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	// if layers are avalible, store them.
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	// for each layer check if it exists in the list.
	for (const char* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
			return false;
	}

	return true;
}

// check for extensions and return a list (based on if validation layers are enabled)
std::vector<const char*> Renderer::getExtensions()
{
	std::vector<const char*> extensions;

	// find extensions
	unsigned int glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (unsigned int i = 0; i < glfwExtensionCount; i++)
	{
		extensions.push_back(glfwExtensions[i]);
	}

	if (enableValidationLayers)
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	return extensions;
}

// check for device capability
bool Renderer::isDeviceSuitable(VkPhysicalDevice device, const bool AMD)
{
	// check for physical properties
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures = {};
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	amdGPU = AMD;

	// Don't choose 1080
	if (AMD)
	{
		char *output = NULL;
		output = strstr(deviceProperties.deviceName, "GTX");
		if (output)
		{
			
			printf("1080 Found - Skipping\n");
			return false;
		}
	}

	// is this a discrete gpu and does it have geom capabilities 
	bool physical = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		deviceFeatures.geometryShader;

	QueueFamilyIndices indices = findQueuesFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);

	bool swapChainAdequate = false;
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	timestampPeriod = deviceProperties.limits.timestampPeriod;
	return physical && indices.isComplete() && extensionsSupported && swapChainAdequate && deviceFeatures.samplerAnisotropy;
}

// check if all extensions include required ones
bool Renderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	// get extensions
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	// create list of availble extensions
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}  


// query info
uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	// query info about the avaliable types of memory
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	// check and return index if desired properties are found.
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to fins suitable memory type!");
}

VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char * layerPrefix, const char * msg, void * userData)
{
	std::cerr << "validation layer: " << msg << std::endl;

	// VkDebugReportFlagsEXT - type of message.
	// objtype - stores enum to device subject of the message (obj)
	// msg is pointer to itself.
	// userdata to pass data to the callback.

	// you should always return false, as it only returns true if a call has been aborted.
	return VK_FALSE;
}

// have to look up callback address using vkGetInstanceProcAddr.
// create own proxy function that handles this in the background
VkResult Renderer::CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	
	if (func != nullptr) 
		return func(instance, pCreateInfo, pAllocator, pCallback);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	
}

// clear memory
void Renderer::DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	
	if (func != nullptr)
		func(instance, callback, pAllocator);

}

// find and return the queuefamilies that support graphics 
QueueFamilyIndices Renderer::findQueuesFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	// as before, find them, set them
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	// find suitable family that supports graphics.
	unsigned int i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		// check for avail queue families
		int queueCount = queueFamily.queueCount;
		if (queueCount > 0)
		{
			queueCount--;
			// don't use same queue fam twice - unlesss it's got multiple queues
			// check for drawing command support
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.graphicsFamily = i;

			// check for compute support & if unused queue
			if (queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
				indices.computeFamily = i;

			// check for presentation support to window
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (presentSupport)
				indices.presentFamily = i;

		}

		// check all have been assigned
		if (indices.isComplete())
			break;

		i++;
	}

	return indices;
}
 
// check swapchain support for surface format, presentation mode, swap extent.
SwapChainSupportDetails Renderer::querySwapChainSupport(VkPhysicalDevice device) {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

// format: contains a format (Specifying the colour channels and types) and colourspace.
VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	// use SRGB for colour space - more accurate percieved colours. 
	// But use standard RGB for the colour format.

	// if surface has no prefered format then it is set as undefined. (therefore free to choose any)
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
		return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };


	// if not, search for avalible formatrs.
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return availableFormat;
	}

	// if none is found that is suitable, just use the first one.
	return availableFormats[0];
}

// Actual conditions for displaying to the screen !!!!
VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes)
{
	// this is always guaranteed to be avalible. -- SIMILAR TO VSYNC. - takes a frame when the display is refreshed and the program adds frames to the end of the queue.
	// blocks Renderer when display queue is full (double buffering)
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& availablePresentMode : availablePresentModes)
	{
		 
		// triple buffering!! - avoid tearing less than std vsync. 
		// doesn't block app when queue is full, the images are replaced with newer ones. (Frames = images)
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return availablePresentMode;
		}
		// immediately transfered to screen when rendered - could cause TEARING
		// prefer this over fifo as some drivers don't support it properly.
		else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			bestMode = availablePresentMode;
		}
		   
	}

	return bestMode;
}

// Swap extent is the resolution of the swap chain images - try to match the window
VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);

	// some wm allow whatever
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	// clamp the values to the max and min values of the capabilities.
	else
	{
		VkExtent2D actualExtent = { width, height };

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

void Renderer::setVertexData(const std::vector<Vertex> vert, const std::vector<uint16_t> ind, const std::vector<particle> part)
{
	// copy data
	dynamic_cast<VertexBO*>(sim->buffers[VERTEX])->vertices = vert;
	dynamic_cast<IndexBO*>(sim->buffers[INDEX])->indices = ind;
	dynamic_cast<InstanceBO*>(sim->buffers[INSTANCE])->particles = part;

}

void Renderer::createComputeUBO()
{
	compute->ubo.particleCount = PARTICLE_COUNT;
	compute->ubo.deltaT = 0.016f;
	compute->ubo.destX = 0.5f;
	compute->ubo.destY = 0.0f;

	VkDeviceSize bufferSize = sizeof(ComputeConfig::computeUBO);
	createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		compute->uniformBuffer, compute->uboMem);  // buffer pointer and memory
}

void Renderer::prepareCompute()
{
	createComputeUBO();

	// create compute pipeline
	// Compute pipelines are created separate from graphics pipelines even if they use the same queue (family index)

	VkDescriptorSetLayoutBinding positionBinding{};
	positionBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	positionBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;;
	positionBinding.binding = 0;
	positionBinding.descriptorCount = 1;

	VkDescriptorSetLayoutBinding uniformBinding{};
	uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniformBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	uniformBinding.binding = 1;
	uniformBinding.descriptorCount = 1;

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
	
	// Binding 0 : Particle position storage buffer
	setLayoutBindings.push_back(positionBinding);
	// Binding 1 : Uniform buffer
	setLayoutBindings.push_back(uniformBinding);


	// decsriptor layout create info
	VkDescriptorSetLayoutCreateInfo descriptorLayout{};
	descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayout.pBindings = setLayoutBindings.data();
	descriptorLayout.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());

	// create descriptor layout
	if(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &compute->descriptorSetLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create compute desc layout");

	// create pipeline layout 
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &compute->descriptorSetLayout;

	if(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &compute->pipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create compute pipeline");

	// create desc sets
	sim->createDescriptorSets();
	
	// Create pipeline //

	// create shader module
	auto computeShaderCode = readFile("res/shaders/comp.spv");

	// modules only needed in creation of pipeline so can be destroyed locally
	VkShaderModule computeShaderMod = createShaderModule(computeShaderCode);

	// assign the shaders
	VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
	compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compShaderStageInfo.module = computeShaderMod;
	compShaderStageInfo.pName = "main";		// function to invoke


	// create info for pipeline setting shader
	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.layout = compute->pipelineLayout;
	computePipelineCreateInfo.flags = 0;
	computePipelineCreateInfo.stage = compShaderStageInfo;
	
	// create it
	if(vkCreateComputePipelines(device, pipeCache, 1, &computePipelineCreateInfo, nullptr, &compute->pipeline) != VK_SUCCESS)
		throw std::runtime_error("failed creating compute pipeline");

	// Create a command buffer for compute operations
	sim->allocateComputeCommandBuffers();

	vkDestroyShaderModule(device, computeShaderMod, nullptr);

	// Build a single command buffer containing the compute dispatch commands
	sim->recordComputeCommands();
}

void Renderer::updateCompute()
{

	auto newTime = std::chrono::system_clock::now();
	float frameTime = std::chrono::duration_cast<std::chrono::milliseconds>(newTime - currentTime).count(); 
	currentTime = newTime;  
	 
	compute->ubo.deltaT = (frameTimer);
	compute->ubo.destX = 0.75f; 
	compute->ubo.destY = 0.0f;
	vkMapMemory(device, compute->uboMem, 0, sizeof(compute->ubo), 0, &compute->mapped);
	memcpy(compute->mapped, &compute->ubo, sizeof(compute->ubo));
	vkUnmapMemory(device, compute->uboMem);
 
}

void Renderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	// create struct as usual
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	// how big is the buffer (enough to store the size of 1 obj * how many)
	bufferInfo.size = size;

	// what purpose is this buffer going to be used for ( can use multiple things)
	bufferInfo.usage = usage;

	// buffer only used by the graphics queue so it can be exclusive
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// create it
	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create buffer");
	}

	// Memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	// determine memory type to allocate 
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	// allocate memory
	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate buffer memory");

	// bind memory to the buffer (int is offset)
	vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkCommandBuffer Renderer::beginSingleTimeCommands()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = gfxCommandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void Renderer::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(device, gfxCommandPool, 1, &commandBuffer);
}

void Renderer::copyBuffer(VkBuffer srcBuff, VkBuffer targetBuff, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuff, targetBuff, 1, &copyRegion);

	endSingleTimeCommands(commandBuffer);

}
