#include "VulkanMgr.h"

#include "logger.h"
#include "imguigameboyx_config.h"

#include <format>

void VulkanMgr::RenderFrame() {
	if (vkResetFences(device, 1, &fence) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] reset fence");
	}

	u32 image_index = 0;
	if (vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, nullptr, fence, &image_index) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] acquire image");
	} 

	if (vkResetCommandPool(device, commandPool, 0) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] reset command pool");
	}

	VkCommandBufferBeginInfo commandbuffer_info = {};
	commandbuffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandbuffer_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(commandBuffer, &commandbuffer_info) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] begin command buffer");
	}
	{
		/*
		// render commands
		VkRenderPassBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_info.renderPass = renderPass;
		begin_info.framebuffer = frameBuffers[image_index];
		begin_info.renderArea = { {0, 0}, {width, height} };
		begin_info.clearValueCount = 1;
		begin_info.pClearValues = &clearColor;
		vkCmdBeginRenderPass(commandBuffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
		*/
	}
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] end command buffer");
	}

	if (vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] wait for fences");
	}
	if (vkResetFences(device, 1, &fence) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] reset fences");
	}

	// submit buffer to queue
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &commandBuffer;
	if (vkQueueSubmit(queue, 1, &submit_info, fence) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] submit command buffer to queue");
	}

	WaitIdle();

	// present
	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pSwapchains = &swapchain;
	present_info.swapchainCount = 1;
	present_info.pImageIndices = &image_index;
	if (vkQueuePresentKHR(queue, &present_info) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] queue present");
	}
}

bool VulkanMgr::InitVulkan(std::vector<const char*>& _sdl_extensions, std::vector<const char*>& _device_extensions) {
	if (!InitVulkanInstance(_sdl_extensions)) {
		return false;
	}
	if (!InitPhysicalDevice()) {
		return false;
	}
	if (!InitLogicalDevice(_device_extensions)) {
		return false;
	}

	return true;
}

bool VulkanMgr::InitVulkanInstance(std::vector<const char*>& _sdl_extensions) {
	u32 vk_layer_property_count;
	if (vkEnumerateInstanceLayerProperties(&vk_layer_property_count, nullptr) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] enumerate layer properties");
		return false;
	}
	auto vk_layer_properties = std::vector<VkLayerProperties>(vk_layer_property_count);
	if (vkEnumerateInstanceLayerProperties(&vk_layer_property_count, vk_layer_properties.data()) != VK_SUCCESS) { return false; }

	u32 vk_extension_count;
	if (vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_count, nullptr) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] enumerate extension properties");
		return false;
	}
	auto vk_extension_properties = std::vector<VkExtensionProperties>(vk_extension_count);
	if (vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_count, vk_extension_properties.data()) != VK_SUCCESS) { return false; }

	VkApplicationInfo vk_app_info = {};
	vk_app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vk_app_info.pApplicationName = APP_TITLE.c_str();
	vk_app_info.applicationVersion = VK_MAKE_VERSION(GBX_VERSION_MAJOR, GBX_VERSION_MINOR, GBX_VERSION_PATCH);
	vk_app_info.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo vk_create_info = {};
	vk_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vk_create_info.pApplicationInfo = &vk_app_info;
	vk_create_info.enabledLayerCount = (u32)VK_ENABLED_LAYERS.size();
	vk_create_info.ppEnabledLayerNames = VK_ENABLED_LAYERS.data();
	vk_create_info.enabledExtensionCount = (u32)_sdl_extensions.size();
	vk_create_info.ppEnabledExtensionNames = _sdl_extensions.data();

	if (vkCreateInstance(&vk_create_info, nullptr, &instance) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] create vulkan instance");
		return false;
	}

	for (const auto& n : VK_ENABLED_LAYERS) {
		LOG_INFO("[vulkan] ", n, " layer enabled");
	}
	for (const auto& n : _sdl_extensions) {
		LOG_INFO("[vulkan] ", n, " extension enabled");
	}

	return true;
}

bool VulkanMgr::InitPhysicalDevice() {
	u32 device_num;
	if (vkEnumeratePhysicalDevices(instance, &device_num, nullptr) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] enumerate physical devices");
		return false;
	}

	if (device_num == 0) {
		LOG_ERROR("[vulkan] no GPUs supported by Vulkan found");
		physicalDevice = nullptr;
		return false;
	}

	auto vk_physical_devices = std::vector<VkPhysicalDevice>(device_num);
	if (vkEnumeratePhysicalDevices(instance, &device_num, vk_physical_devices.data()) != VK_SUCCESS) { return false; }

	LOG_INFO(device_num, " GPU(s) found:");
	for (const auto& n : vk_physical_devices) {
		VkPhysicalDeviceProperties vk_phys_dev_prop = {};
		vkGetPhysicalDeviceProperties(n, &vk_phys_dev_prop);
		LOG_INFO(vk_phys_dev_prop.deviceName);
	}

	physicalDevice = vk_physical_devices[0];
	vkGetPhysicalDeviceProperties(vk_physical_devices[0], &physicalDeviceProperties);
	SetGPUInfo();
	LOG_INFO(physicalDeviceProperties.deviceName, " (", driverVersion, ") selected");

	return true;
}

bool VulkanMgr::InitLogicalDevice(std::vector<const char*>& _device_extensions) {
	u32 queue_families_num;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_families_num, nullptr);
	auto queue_family_properties = std::vector<VkQueueFamilyProperties>(queue_families_num);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_families_num, queue_family_properties.data());

	u32 graphics_queue_index = 0;
	for (u32 i = 0; const auto & n : queue_family_properties) {
		if ((n.queueCount > 0) && (n.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			graphics_queue_index = i;
		}
		i++;
	}

	const float queue_priorities[] = { 1.f };
	VkDeviceQueueCreateInfo queue_create_info = {};
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = graphics_queue_index;
	queue_create_info.queueCount = 1;
	queue_create_info.pQueuePriorities = queue_priorities;

	VkPhysicalDeviceFeatures vk_enabled_features = {};

	VkDeviceCreateInfo vk_device_info = {};
	vk_device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	vk_device_info.queueCreateInfoCount = 1;
	vk_device_info.pQueueCreateInfos = &queue_create_info;
	vk_device_info.enabledExtensionCount = (u32)_device_extensions.size();
	vk_device_info.ppEnabledExtensionNames = _device_extensions.data();
	vk_device_info.pEnabledFeatures = &vk_enabled_features;

	if (vkCreateDevice(physicalDevice, &vk_device_info, nullptr, &device) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] create logical device");
		return false;
	}

	familyIndex = graphics_queue_index;
	vkGetDeviceQueue(device, graphics_queue_index, 0, &queue);

	return true;
}

void VulkanMgr::SetGPUInfo() {
	u16 vendor_id = physicalDeviceProperties.vendorID & 0xFFFF;
	vendor = get_vendor(vendor_id);

	u32 driver = physicalDeviceProperties.driverVersion;
	if (vendor_id == ID_NVIDIA) {
		driverVersion = std::format("{}.{}", (driver >> 22) & 0x3ff, (driver >> 14) & 0xff);
	}
	else if (vendor_id == ID_AMD) {
		driverVersion = std::format("{}.{}", driver >> 14, driver & 0x3fff);
	}
	else {
		driverVersion = "";
	}
}

bool VulkanMgr::InitSurface() {
	if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
		LOG_ERROR("[vulkan] ", SDL_GetError());
		return false;
	}
	else {
		return true;
	}
}

bool VulkanMgr::InitSwapchain(VkImageUsageFlags _flags) {
	VkBool32 supports_present = 0;
	if ((vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface, &supports_present) != VK_SUCCESS) || !supports_present) {
		LOG_ERROR("[vulkan] graphics queue doesn't support present");
		return false;
	}

	u32 formats_num;
	if ((vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formats_num, nullptr) != VK_SUCCESS) || !formats_num) {
		LOG_ERROR("[vulkan] couldn't acquire image formats");
		return false;
	}
	auto image_formats = std::vector<VkSurfaceFormatKHR>(formats_num);
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formats_num, image_formats.data()) != VK_SUCCESS) { return false; }

	// TODO: look into formats and which to use
	format = image_formats[0].format;
	colorSpace = image_formats[0].colorSpace;

	VkSurfaceCapabilitiesKHR surface_capabilites;
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surface_capabilites) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] couldn't acquire surface capabilities");
		return false;
	}
	if (surface_capabilites.currentExtent.width == 0xFFFFFFFF) {
		surface_capabilites.currentExtent.width = surface_capabilites.minImageExtent.width;
	}
	if (surface_capabilites.currentExtent.height == 0xFFFFFFFF) {
		surface_capabilites.currentExtent.height = surface_capabilites.minImageExtent.height;
	}
	// unlimited images
	if (surface_capabilites.maxImageCount == 0) {
		surface_capabilites.maxImageCount = 8;
	}

	presentMode = VK_PRESENT_MODE_FIFO_KHR;
	minImageCount = 3;

	// TODO: look into swapchain settings
	VkSwapchainCreateInfoKHR swapchain_info;
	swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_info.flags = 0;
	swapchain_info.surface = surface;
	swapchain_info.minImageCount = minImageCount;							// triple buffering
	swapchain_info.imageFormat = format;									// bit depth/format
	swapchain_info.imageColorSpace = colorSpace;							// used color space
	swapchain_info.imageExtent = surface_capabilites.currentExtent;			// image size
	swapchain_info.imageArrayLayers = 1;									// multiple images at once (e.g. VR)
	swapchain_info.imageUsage = _flags;										// usage of swapchain
	swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;			// sharing between different queues
	swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;	// no transform
	swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;		// no transparency (opaque)
	swapchain_info.presentMode = presentMode;								// simple image fifo (vsync)
	swapchain_info.clipped = VK_FALSE;										// draw only visible window areas if true (clipping)
	swapchain_info.oldSwapchain = VK_NULL_HANDLE;
	swapchain_info.pNext = nullptr;

	if (vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] couldn't create swapchain");
		return false;
	}

	width = surface_capabilites.currentExtent.width;
	height = surface_capabilites.currentExtent.height;

	u32 image_num = 0;
	if (vkGetSwapchainImagesKHR(device, swapchain, &image_num, nullptr) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] couldn't acquire image count");
		return false;
	}

	images.resize(image_num);
	if (vkGetSwapchainImagesKHR(device, swapchain, &image_num, images.data()) != VK_SUCCESS) { return false; }

	imageViews.resize(image_num);
	for (u32 i = 0; i < image_num; i++) {
		VkImageViewCreateInfo imageview_info = {};
		imageview_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageview_info.image = images[i];
		imageview_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageview_info.format = format;
		imageview_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY };
		imageview_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		if (vkCreateImageView(device, &imageview_info, nullptr, &imageViews[i]) != VK_SUCCESS) {
			LOG_ERROR("[vulkan] create image view ", i);
			return false;
		}
	}

	return true;
}

bool VulkanMgr::InitRenderPass() {
	VkAttachmentDescription attachment_description = {};
	attachment_description.format = format;
	attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;												// no MSAA
	attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;										// clear previous
	attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;										// store result
	attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;									// before renderpass -> don't care
	attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;								// final format to present

	VkAttachmentReference attachment_reference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };		// format for rendering
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;										// pass type: graphics
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &attachment_reference;

	VkRenderPassCreateInfo renderpass_info = {};
	renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpass_info.attachmentCount = 1;
	renderpass_info.pAttachments = &attachment_description;
	renderpass_info.subpassCount = 1;
	renderpass_info.pSubpasses = &subpass;

	if (vkCreateRenderPass(device, &renderpass_info, nullptr, &renderPass) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] create renderpass");
		return false;
	}

	return true;
}

bool VulkanMgr::InitFrameBuffers() {
	frameBuffers.resize(images.size());
	for (u32 i = 0; i < images.size(); i++) {
		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = renderPass;
		framebuffer_info.attachmentCount = 1;
		framebuffer_info.pAttachments = &imageViews[i];
		framebuffer_info.width = width;
		framebuffer_info.height = height;
		framebuffer_info.layers = 1;

		if (vkCreateFramebuffer(device, &framebuffer_info, nullptr, &frameBuffers[i]) != VK_SUCCESS) {
			LOG_ERROR("[vulkan] create framebuffer ", i);
			return false;
		}
	}

	return true;
}

bool VulkanMgr::InitCommandBuffers() {
	VkCommandPoolCreateInfo cmdpool_info = {};
	cmdpool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdpool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;					// command buffers only valid for one frame
	cmdpool_info.queueFamilyIndex = familyIndex;
	if (vkCreateCommandPool(device, &cmdpool_info, nullptr, &commandPool) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] create command pool");
		return false;
	}

	VkFenceCreateInfo fence_info = {};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if (vkCreateFence(device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] create fence");
		return false;
	}

	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = commandPool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;
	if (vkAllocateCommandBuffers(device, &allocate_info, &commandBuffer) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] allocate command buffers");
		return false;
	}

	return true;
}

bool VulkanMgr::ExitVulkan() {
	WaitIdle();

	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);
	
	return true;
}

void VulkanMgr::DestroySwapchain() {
	WaitIdle();
	for (auto& n : imageViews) {
		vkDestroyImageView(device, n, nullptr);
	}
	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void VulkanMgr::DestroySurface() {
	WaitIdle();
	vkDestroySurfaceKHR(instance, surface, nullptr);
}

void VulkanMgr::DestroyFrameBuffers() {
	WaitIdle();
	for (auto& n : frameBuffers) {
		vkDestroyFramebuffer(device, n, nullptr);
	}
}

void VulkanMgr::DestroyRenderPass() {
	WaitIdle();
	vkDestroyRenderPass(device, renderPass, nullptr);
}

void VulkanMgr::DestroyCommandBuffer() {
	WaitIdle();
	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyFence(device, fence, nullptr);
}

void VulkanMgr::WaitIdle() {
	if (vkDeviceWaitIdle(device) != VK_SUCCESS) {
		LOG_ERROR("[vulkan] GPU wait idle");
	}
}