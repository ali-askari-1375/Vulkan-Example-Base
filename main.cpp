
#include <iostream>
#include <vector>
#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <bitset>
#include <algorithm>
#include <iterator>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <shellscalingapi.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>


HINSTANCE G_Hinstance = {};
HWND G_Hwnd = {};

constexpr std::uint32_t G_PreferredImageCount = 2;
constexpr std::uint32_t G_MaxFramesInFlight = 2;
std::uint32_t G_CurrentFrame = 0;

vk::DispatchLoaderDynamic G_DLD = {};

vk::Instance G_VkInstance = {};
vk::PhysicalDevice G_PhysicalDevice = {};
vk::SampleCountFlagBits G_SampleCount = vk::SampleCountFlagBits::e1;

std::optional<std::uint32_t> G_GraphicsQueueFamilyIndex;
std::optional<std::uint32_t> G_PresentQueueFamilyIndex;
std::optional<std::uint32_t> G_ComputeQueueFamilyIndex;

vk::Device G_Device = {};
vk::Queue G_GraphicsQueue;
vk::Queue G_PresentQueue;
vk::Queue G_ComputeQueue;

vk::CommandPool G_DynamicCommandPool = {};
vk::CommandPool G_StaticCommandPool = {};

std::array<vk::CommandBuffer, G_MaxFramesInFlight> G_CommandBuffers = {};

std::array<bool, G_MaxFramesInFlight> G_WaitForFences = {};
std::array<vk::Fence, G_MaxFramesInFlight> G_InFlightFences = {};
std::array<vk::Semaphore, G_MaxFramesInFlight> G_ImageAvailableSemaphores = {};
std::array<vk::Semaphore, G_MaxFramesInFlight> G_RenderFinishedSemaphores = {};

vk::SurfaceKHR G_Surface = {};
vk::SurfaceFormatKHR G_SurfaceFormat = {};
vk::PresentModeKHR G_SurfacePresentMode = {};
std::uint32_t G_SurfaceImageCount = {};

vk::RenderPass G_RenderPass = {};

bool G_SwapchainOK = false;
vk::Extent2D G_WindowSize = {};
vk::Extent2D G_SwapchainExtent = {};

vk::SwapchainKHR G_Swapchain = {};

std::vector<vk::Image> G_ColorBufferImages;
std::vector<vk::DeviceMemory> G_ColorBufferDeviceMemories;
std::vector<vk::ImageView> G_ColorBufferImageViews;

std::vector<vk::Image> G_SwapchainImages = {};
std::vector<vk::ImageView> G_SwapchainImageViews = {};

std::vector<vk::Framebuffer> G_Framebuffers = {};


void InitWindow();
void ShutdownWindow();

void Render();

std::uint32_t FindMemoryTypeIndex(std::uint32_t typeFilter, vk::MemoryPropertyFlags Properties);

void InitVulkan();
void ShutdownVulkan();

void InitPhysicalDevice();
void InitQueueFamilies();
void InitDevice();
void InitQueues();

void InitCommandPools();
void InitCommandBuffers();
void InitSyncObjects();

void InitSurface();

void InitRenderPass();

void InitSwapchain();
void CreateSwapchain();
void DestroySwapchain();
void RecreateSwapchain();
void ShutdownSwapchain();

LRESULT CALLBACK WndProc(HWND Hwnd, UINT Msg, WPARAM Wparam, LPARAM Lparam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR szCmdLine, int nCmdShow)
{
	::SetProcessDpiAwareness(PROCESS_DPI_UNAWARE);

	G_Hinstance = hInstance;

	InitWindow();
	InitVulkan();

	::ShowWindow(G_Hwnd, SW_SHOW);
	::UpdateWindow(G_Hwnd);


	MSG Msg = {};
	bool bContinue = true;
	while(bContinue) {

		for (std::uint32_t i = 0; i < std::uint32_t(100); i++) {
			if (::PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE)) {
				if (Msg.message == WM_QUIT) {
					bContinue = false;
					break;
				}
				::TranslateMessage(&Msg);
				::DispatchMessage(&Msg);
			} else {
				break;
			}
		}


		if (G_SwapchainOK) {
			Render();
		} else {
			RecreateSwapchain();
			if (G_SwapchainOK) Render();
		}

	}

	ShutdownVulkan();
	ShutdownWindow();

	return 0;
}

LRESULT CALLBACK WndProc(HWND Hwnd, UINT Msg, WPARAM Wparam, LPARAM Lparam)
{
	switch(Msg)
	{
	case WM_ERASEBKGND:
	{
		return 1;
	}
	break;
	case WM_DESTROY:
	{
		::PostQuitMessage(0);
	}
	break;
	}

	return DefWindowProcA(Hwnd, Msg, Wparam, Lparam);
}



void InitWindow()
{

	static constexpr DWORD dwStyle = WS_CAPTION | WS_BORDER | WS_MINIMIZEBOX | WS_SYSMENU | WS_SIZEBOX | WS_MAXIMIZEBOX;
	RECT rc = RECT{0, 0, 800, 600};
	::AdjustWindowRectEx(&rc, dwStyle, FALSE, 0);

	const WNDCLASSEXA wcx = WNDCLASSEXA{
		sizeof(WNDCLASSEXA),
		0,
		WndProc,
		0,
		0,
		G_Hinstance,
		LoadIconA(nullptr, IDI_APPLICATION),
		LoadCursorA(nullptr, IDC_ARROW),
		0,
		0,
		"TesselTestClass",
		LoadIconA(nullptr, IDI_APPLICATION)
	};
	::RegisterClassExA(&wcx);


	G_Hwnd = ::CreateWindowExA(
		0,
		"TesselTestClass",
		"TesselTest",
		dwStyle,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		G_Hinstance,
		nullptr
	);
}

void ShutdownWindow()
{
	::UnregisterClassA(
		"TesselTestClass",
		G_Hinstance
	);
}


void Render()
{
	if (!G_SwapchainOK) return;

	if (G_WaitForFences[G_CurrentFrame]) {
		(void)G_Device.waitForFences(1, &G_InFlightFences[G_CurrentFrame], VK_TRUE, std::numeric_limits<std::uint64_t>::max(), G_DLD);
		G_WaitForFences[G_CurrentFrame] = false;
	} else {
		G_GraphicsQueue.waitIdle(G_DLD);
	}
	(void)G_Device.resetFences(1, &G_InFlightFences[G_CurrentFrame], G_DLD);

	std::uint32_t ImageIndex = 0;
	try {
		vk::ResultValue Acquire = G_Device.acquireNextImageKHR(
			G_Swapchain, std::numeric_limits<std::uint64_t>::max(),
			G_ImageAvailableSemaphores[G_CurrentFrame], nullptr,
			G_DLD
			);

		ImageIndex = Acquire.value;
	}
	catch (...) {
		G_SwapchainOK = false;
		return;
	}

	vk::CommandBuffer CommandBuffer = G_CommandBuffers[G_CurrentFrame];
	CommandBuffer.reset({}, G_DLD);

	const vk::CommandBufferBeginInfo commandBufferBeginInfo = {};
	CommandBuffer.begin(commandBufferBeginInfo, G_DLD);

	static float __Color[4];
	std::memcpy(__Color, DirectX::Colors::SpringGreen.f, sizeof(__Color));

	const vk::ClearColorValue clearColor = { __Color[0], __Color[1], __Color[2], __Color[3] };
	const vk::ClearValue clearValues[1] = {clearColor};
	const vk::RenderPassBeginInfo renderPassBeginInfo = vk::RenderPassBeginInfo(G_RenderPass, G_Framebuffers[ImageIndex], vk::Rect2D(vk::Offset2D(0,0), G_SwapchainExtent), 1, clearValues);

	CommandBuffer.beginRenderPass(&renderPassBeginInfo, vk::SubpassContents::eInline, G_DLD);

	CommandBuffer.endRenderPass(G_DLD);

	CommandBuffer.end(G_DLD);

	const vk::Semaphore waitSemaphores[] = { G_ImageAvailableSemaphores[G_CurrentFrame]};
	const vk::Semaphore signalSemaphores[] = { G_RenderFinishedSemaphores[G_CurrentFrame]};
	static constexpr vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	const vk::SubmitInfo submitInfo = vk::SubmitInfo(1, waitSemaphores, waitStages, 1, &CommandBuffer, 1, signalSemaphores);
	try {
		G_WaitForFences[G_CurrentFrame] = true;
		G_GraphicsQueue.submit(submitInfo, G_InFlightFences[G_CurrentFrame], G_DLD);
	}
	catch (...) {
		G_WaitForFences[G_CurrentFrame] = false;
		G_SwapchainOK = false;
		return;
	}

	const vk::SwapchainKHR Swapchains[1] = {G_Swapchain};
	const vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR(1, signalSemaphores, 1, Swapchains, &ImageIndex);
	try {
		(void)G_PresentQueue.presentKHR(presentInfo, G_DLD);
	}
	catch (...) {
		G_SwapchainOK = false;
		return ;
	}

	G_CurrentFrame = (G_CurrentFrame + 1) % G_MaxFramesInFlight;
}


std::uint32_t FindMemoryTypeIndex(std::uint32_t typeFilter, vk::MemoryPropertyFlags Properties)
{
	vk::PhysicalDeviceMemoryProperties MemProperties = G_PhysicalDevice.getMemoryProperties(G_DLD);

	for (std::uint32_t i = 0; i < MemProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && MemProperties.memoryTypes[i].propertyFlags == Properties) {
			return i;
		}
	}

	for (std::uint32_t i = 0; i < MemProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (MemProperties.memoryTypes[i].propertyFlags & Properties) == Properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void InitVulkan()
{
	G_DLD.init();

	const std::uint32_t VkApiVersion = vk::enumerateInstanceVersion(G_DLD) & (~std::uint32_t(0xFFFU));
	const vk::ApplicationInfo AppInfo = vk::ApplicationInfo(
		"Baldey",
		VK_MAKE_API_VERSION(0, 1, 0, 0),
		"BaldeyEngine",
		VK_MAKE_API_VERSION(0, 1, 0, 0),
		VkApiVersion
		);

	static constexpr std::array<const char*, 1> RequiredInstanceLayers = {
		"VK_LAYER_KHRONOS_validation",
	};
	static constexpr std::array<const char*, 3> RequiredInstanceExtensions = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	};
	const std::vector<vk::LayerProperties> SupportedInstanceLayers = vk::enumerateInstanceLayerProperties(G_DLD);
	const std::vector<vk::ExtensionProperties> SupportedInstanceExtensions = vk::enumerateInstanceExtensionProperties(nullptr, G_DLD);

	for (const char* RequiredLayer : RequiredInstanceLayers) {
		if (std::find_if(SupportedInstanceLayers.begin(), SupportedInstanceLayers.end(), [&RequiredLayer](const vk::LayerProperties& LayerProperties) {return std::strcmp(LayerProperties.layerName.data(), RequiredLayer) == 0; }) == SupportedInstanceLayers.end())
			throw std::runtime_error("Vulkan doesn't support required layers");
	}
	for (const char* RequiredExtension : RequiredInstanceExtensions) {
		if (std::find_if(SupportedInstanceExtensions.begin(), SupportedInstanceExtensions.end(), [&RequiredExtension](const vk::ExtensionProperties& ExtensionProperties) {return std::strcmp(ExtensionProperties.extensionName.data(), RequiredExtension) == 0; }) == SupportedInstanceExtensions.end())
			throw std::runtime_error("Vulkan doesn't support required extensions");
	}

	const vk::InstanceCreateInfo VkInstanceCI = vk::InstanceCreateInfo(
		{},
		&AppInfo,
		static_cast<std::uint32_t>(RequiredInstanceLayers.size()), RequiredInstanceLayers.data(),
		static_cast<std::uint32_t>(RequiredInstanceExtensions.size()), RequiredInstanceExtensions.data()
		);
	G_VkInstance = vk::createInstance(VkInstanceCI, nullptr, G_DLD);
	G_DLD.init(G_VkInstance);


	InitPhysicalDevice();
	InitQueueFamilies();
	InitDevice();
	InitQueues();
	InitCommandPools();
	InitCommandBuffers();
	InitSyncObjects();
	InitSurface();
	InitRenderPass();

	InitSwapchain();
}

void ShutdownVulkan()
{
	if (G_Device) {
		G_Device.waitIdle(G_DLD);
		ShutdownSwapchain();


		if (G_RenderPass) {
			G_Device.destroyRenderPass(G_RenderPass, nullptr, G_DLD);
			G_RenderPass = nullptr;
		}

		if (G_Surface) {
			G_VkInstance.destroySurfaceKHR(G_Surface, nullptr, G_DLD);
			G_Surface = nullptr;
		}

		for (auto& Item : G_InFlightFences) {
			if (Item) {
				G_Device.destroyFence(Item, nullptr, G_DLD);
				Item = nullptr;
			}
		}

		for (auto& Item : G_RenderFinishedSemaphores) {
			if (Item) {
				G_Device.destroySemaphore(Item, nullptr, G_DLD);
				Item = nullptr;
			}
		}

		for (auto& Item : G_ImageAvailableSemaphores) {
			if (Item) {
				G_Device.destroySemaphore(Item, nullptr, G_DLD);
				Item = nullptr;
			}
		}

		for (auto& Item : G_CommandBuffers) {
			if (Item) {
				G_Device.freeCommandBuffers(G_DynamicCommandPool, Item, G_DLD);
				Item = nullptr;
			}
		}

		if (G_DynamicCommandPool) {
			G_Device.destroyCommandPool(G_DynamicCommandPool, nullptr, G_DLD);
			G_DynamicCommandPool = nullptr;
		}

		if (G_StaticCommandPool) {
			G_Device.destroyCommandPool(G_StaticCommandPool, nullptr, G_DLD);
			G_StaticCommandPool = nullptr;
		}

		G_Device.destroy(nullptr, G_DLD);
		G_Device = nullptr;
	}

	if (G_VkInstance) {
		G_VkInstance.destroy(nullptr, G_DLD);
		G_VkInstance = nullptr;
	}
}

void InitPhysicalDevice()
{
	const std::vector<vk::PhysicalDevice> AvailableDevices = G_VkInstance.enumeratePhysicalDevices(G_DLD);
	static constexpr std::array<const char*, 1> RequiredDeviceLayers = {
		"VK_LAYER_KHRONOS_validation",
	};
	static constexpr std::array<const char*, 1> RequiredDeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	for (auto PhysDevice : AvailableDevices) {
		bool bAllExtensionsSupported = true;
		bool bAllLayersSupported = true;

		const std::vector<vk::ExtensionProperties> SupportedDeviceExtensions = PhysDevice.enumerateDeviceExtensionProperties(nullptr, G_DLD);
		const std::vector<vk::LayerProperties> SupportedDeviceLayers = PhysDevice.enumerateDeviceLayerProperties(G_DLD);

		for (const char* RequiredLayer : RequiredDeviceLayers) {
			if (std::find_if(SupportedDeviceLayers.begin(), SupportedDeviceLayers.end(), [&RequiredLayer](const vk::LayerProperties& ExtensionProperties) {return std::strcmp(ExtensionProperties.layerName.data(), RequiredLayer) == 0; }) == SupportedDeviceLayers.end()) {
				bAllLayersSupported = false;
				break;
			}
		}
		for (const char* RequiredExtension : RequiredDeviceExtensions) {
			if (std::find_if(SupportedDeviceExtensions.begin(), SupportedDeviceExtensions.end(), [&RequiredExtension](const vk::ExtensionProperties& ExtensionProperties) {return std::strcmp(ExtensionProperties.extensionName.data(), RequiredExtension) == 0; }) == SupportedDeviceExtensions.end()) {
				bAllExtensionsSupported = false;
				break;
			}
		}
		if (bAllLayersSupported && bAllExtensionsSupported) {
			G_PhysicalDevice = PhysDevice;
			break;
		}
	}

	if (!G_PhysicalDevice)
		throw std::runtime_error("Could not find any suitable physical device");


	const vk::PhysicalDeviceProperties Props = G_PhysicalDevice.getProperties(G_DLD);

	std::cout << ">>>>>>>>>>>>>>>>>> : " << Props.limits.maxTessellationPatchSize << std::endl;

	const vk::SampleCountFlags SampleCountFlags = Props.limits.framebufferColorSampleCounts & Props.limits.framebufferDepthSampleCounts;
	if (SampleCountFlags & vk::SampleCountFlagBits::e8) { G_SampleCount = vk::SampleCountFlagBits::e8; }
	else if (SampleCountFlags & vk::SampleCountFlagBits::e4) { G_SampleCount = vk::SampleCountFlagBits::e4; }
	else if (SampleCountFlags & vk::SampleCountFlagBits::e2) { G_SampleCount = vk::SampleCountFlagBits::e2; }
	else { G_SampleCount = vk::SampleCountFlagBits::e1; }

}

void InitQueueFamilies()
{

	G_GraphicsQueueFamilyIndex.reset();
	G_PresentQueueFamilyIndex.reset();
	G_ComputeQueueFamilyIndex.reset();

	const std::vector<vk::QueueFamilyProperties> QueueFamilies = G_PhysicalDevice.getQueueFamilyProperties(G_DLD);

	std::vector<vk::Bool32> SupportsPresent = std::vector<vk::Bool32>(QueueFamilies.size(), vk::False);
	for (std::uint32_t i = 0; i < QueueFamilies.size(); i++) {
		SupportsPresent[i] = G_PhysicalDevice.getWin32PresentationSupportKHR(i, G_DLD);
	}

	for (std::uint32_t i = 0; i < QueueFamilies.size(); i++) {
		if (QueueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
			if (SupportsPresent[i] == vk::True) {
				G_GraphicsQueueFamilyIndex = static_cast<std::uint32_t>(i);
				G_PresentQueueFamilyIndex = static_cast<std::uint32_t>(i);
				break;
			}
			if (!G_GraphicsQueueFamilyIndex.has_value()) {
				G_GraphicsQueueFamilyIndex = static_cast<std::uint32_t>(i);
			}
		}
	}
	if (!G_PresentQueueFamilyIndex.has_value()) {
		for (std::uint32_t i = 0; i < QueueFamilies.size(); i++) {
			if (SupportsPresent[i] == vk::True) {
				G_PresentQueueFamilyIndex = static_cast<std::uint32_t>(i);
				break;
			}
		}
	}

	for (std::uint32_t i = 0; i < QueueFamilies.size(); i++) {
		if (QueueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) {
			if (!(QueueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)) {
				G_ComputeQueueFamilyIndex = static_cast<std::uint32_t>(i);
				break;
			}
		}
	}

	if (!G_ComputeQueueFamilyIndex.has_value()) {
		for (std::uint32_t i = 0; i < QueueFamilies.size(); i++) {
			if (QueueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) {
				G_ComputeQueueFamilyIndex = static_cast<std::uint32_t>(i);
				break;
			}
		}
	}

	if (!G_GraphicsQueueFamilyIndex.has_value() ||
		!G_PresentQueueFamilyIndex.has_value() ||
		!G_ComputeQueueFamilyIndex.has_value()) {
		throw std::runtime_error("Failed to find all required queue families");
	}

}

void InitDevice()
{
	std::set<std::uint32_t> UniqueIndices;
	UniqueIndices.insert(G_GraphicsQueueFamilyIndex.value());
	UniqueIndices.insert(G_PresentQueueFamilyIndex.value());
	UniqueIndices.insert(G_ComputeQueueFamilyIndex.value());

	std::vector<vk::DeviceQueueCreateInfo> QueueCIs;
	const float QueuePriority = 1.0f;
	for (std::uint32_t QueueFamilyIndex : UniqueIndices) {
		QueueCIs.push_back(
			vk::DeviceQueueCreateInfo(
				{},
				QueueFamilyIndex,
				1,
				&QueuePriority
				)
			);
	}

	static constexpr std::array<const char*, 1> EnabledLayers = {
		"VK_LAYER_KHRONOS_validation",
	};
	static constexpr std::array<const char*, 1> EnabledExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};
	static constexpr vk::PhysicalDeviceFeatures EnabledFeatures = vk::PhysicalDeviceFeatures();

	const vk::DeviceCreateInfo DeviceCI = vk::DeviceCreateInfo(
		{},
		static_cast<std::uint32_t>(QueueCIs.size()), QueueCIs.data(),
		static_cast<std::uint32_t>(EnabledLayers.size()), EnabledLayers.data(),
		static_cast<std::uint32_t>(EnabledExtensions.size()), EnabledExtensions.data(),
		&EnabledFeatures
	);

	G_Device = G_PhysicalDevice.createDevice(DeviceCI, nullptr, G_DLD);
	if (!G_Device)
		throw std::runtime_error("Failed to create logical device");
	G_DLD.init(G_Device);
}

void InitQueues()
{
	G_GraphicsQueue = G_Device.getQueue(G_GraphicsQueueFamilyIndex.value(), 0, G_DLD);
	G_PresentQueue = G_Device.getQueue(G_PresentQueueFamilyIndex.value(), 0, G_DLD);
	G_ComputeQueue = G_Device.getQueue(G_ComputeQueueFamilyIndex.value(), 0, G_DLD);
}

void InitCommandPools()
{
	const vk::CommandPoolCreateInfo DynamicCommandPoolCI = vk::CommandPoolCreateInfo{
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		G_GraphicsQueueFamilyIndex.value()
	};
	G_DynamicCommandPool = G_Device.createCommandPool(DynamicCommandPoolCI, nullptr, G_DLD);

	const vk::CommandPoolCreateInfo StaticCommandPoolCI = vk::CommandPoolCreateInfo{
		{},
		G_GraphicsQueueFamilyIndex.value()
	};
	G_StaticCommandPool = G_Device.createCommandPool(StaticCommandPoolCI, nullptr, G_DLD);
}

void InitCommandBuffers()
{
	const vk::CommandBufferAllocateInfo CommandBufferAI = vk::CommandBufferAllocateInfo(
		G_DynamicCommandPool,
		vk::CommandBufferLevel::ePrimary,
		G_MaxFramesInFlight
	);

	const std::vector<vk::CommandBuffer> CmdBuffers = G_Device.allocateCommandBuffers(CommandBufferAI, G_DLD);
	for (std::uint32_t i = 0; i < G_MaxFramesInFlight; i++) {
		G_CommandBuffers[i] = CmdBuffers[i];
	}
}

void InitSyncObjects()
{
	const vk::SemaphoreCreateInfo SemaphoreCI = {};
	const vk::FenceCreateInfo FenceCI = vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);

	for (std::size_t i = 0; i < G_MaxFramesInFlight; i++) {
		G_ImageAvailableSemaphores[i] = G_Device.createSemaphore(SemaphoreCI, nullptr, G_DLD);
		G_RenderFinishedSemaphores[i] = G_Device.createSemaphore(SemaphoreCI, nullptr, G_DLD);
		G_InFlightFences[i] = G_Device.createFence(FenceCI, nullptr, G_DLD);
		G_WaitForFences[i] = true;
	}
}

void InitSurface()
{
	const vk::Win32SurfaceCreateInfoKHR SurfaceCI = vk::Win32SurfaceCreateInfoKHR{
		{},
		G_Hinstance,
		G_Hwnd,
		{}
	};

	G_Surface = G_VkInstance.createWin32SurfaceKHR(SurfaceCI, nullptr, G_DLD);
	if (!G_Device)
		throw std::runtime_error("Failed to create win32 surface");


	const std::vector<vk::SurfaceFormatKHR> SurfaceFormats = G_PhysicalDevice.getSurfaceFormatsKHR(G_Surface, G_DLD);
	if (SurfaceFormats.empty())
		throw std::runtime_error("The surface doesn't support any format");

	G_SurfaceFormat = SurfaceFormats[0];
	for (auto& SurfaceFormat : SurfaceFormats) {
		if (SurfaceFormat.format == vk::Format::eB8G8R8A8Unorm && SurfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			G_SurfaceFormat = SurfaceFormat;
			break;
		}
	}

	const std::vector<vk::PresentModeKHR> PresentModes = G_PhysicalDevice.getSurfacePresentModesKHR(G_Surface, G_DLD);
	if (PresentModes.empty())
		throw std::runtime_error("The surface doesn't support any present mode");

	G_SurfacePresentMode = PresentModes[0];
	if (G_SurfacePresentMode != vk::PresentModeKHR::eFifo) {
		for (auto& PresentMode : PresentModes) {
			if (PresentMode == vk::PresentModeKHR::eFifo) {
				G_SurfacePresentMode = PresentMode;
				break;
			}
		}
	}
	if (G_SurfacePresentMode != vk::PresentModeKHR::eMailbox && G_SurfacePresentMode != vk::PresentModeKHR::eFifo) {
		for (auto& PresentMode : PresentModes) {
			if (PresentMode == vk::PresentModeKHR::eMailbox) {
				G_SurfacePresentMode = PresentMode;
				break;
			}
		}
	}

	vk::SurfaceCapabilitiesKHR SurfaceCapabilities = G_PhysicalDevice.getSurfaceCapabilitiesKHR(G_Surface, G_DLD);
	G_SurfaceImageCount = std::min(
		SurfaceCapabilities.maxImageCount,
		std::max(SurfaceCapabilities.minImageCount, G_PreferredImageCount)
	);

}

void InitRenderPass()
{

	const std::array<vk::AttachmentDescription, 2> attachments = {
		vk::AttachmentDescription(
			{},
			G_SurfaceFormat.format,
			G_SampleCount,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal
		),
		vk::AttachmentDescription(
			{},
			G_SurfaceFormat.format,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR
		),
	};

	static constexpr std::array<vk::AttachmentReference, 1> ColorAttachmentRefs = {
		vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal),
	};
	static constexpr std::array<vk::AttachmentReference, 1> ResolveAttachmentRefs = {
		vk::AttachmentReference(1, vk::ImageLayout::eColorAttachmentOptimal),
	};

	static constexpr vk::SubpassDescription subpass = vk::SubpassDescription(
		{},
		vk::PipelineBindPoint::eGraphics,
		0,
		nullptr,
		1,
		&ColorAttachmentRefs[0],
		&ResolveAttachmentRefs[0],
		nullptr,
		0,
		nullptr
		);

	const vk::RenderPassCreateInfo renderPassCI = vk::RenderPassCreateInfo(
		{},
		static_cast<std::uint32_t>(attachments.size()),
		attachments.data(),
		1,
		&subpass
		);

	G_RenderPass = G_Device.createRenderPass(renderPassCI, nullptr, G_DLD);
	if (!G_RenderPass)
		throw std::runtime_error("Failed to create multisampled renderpass");
}



void InitSwapchain()
{
	CreateSwapchain();
}

void CreateSwapchain()
{
	if (G_SwapchainOK) {
		DestroySwapchain();
	}

	RECT rc = {};
	::GetClientRect(G_Hwnd, &rc);
	G_WindowSize = vk::Extent2D{static_cast<std::uint32_t>(rc.right - rc.left), static_cast<std::uint32_t>(rc.bottom - rc.top)};

	const vk::SurfaceCapabilitiesKHR SurfaceCapabilities = G_PhysicalDevice.getSurfaceCapabilitiesKHR(G_Surface, G_DLD);

	G_SwapchainExtent = SurfaceCapabilities.currentExtent;
	if (G_SwapchainExtent.width == std::numeric_limits<std::uint32_t>::max()) {
		G_SwapchainExtent.width = G_WindowSize.width;
		G_SwapchainExtent.width = std::min(SurfaceCapabilities.maxImageExtent.width,std::max(SurfaceCapabilities.minImageExtent.width, G_SwapchainExtent.width));
	}
	if (G_SwapchainExtent.height == std::numeric_limits<std::uint32_t>::max()) {
		G_SwapchainExtent.height = G_WindowSize.height;
		G_SwapchainExtent.height = std::min(SurfaceCapabilities.maxImageExtent.height,std::max(SurfaceCapabilities.minImageExtent.height, G_SwapchainExtent.height));
	}

	if (G_SwapchainExtent.width == 0 || G_SwapchainExtent.height == 0) return;

	try {

		if (G_GraphicsQueueFamilyIndex.value() != G_PresentQueueFamilyIndex.value()) {
			const std::uint32_t QueueFamilyIndices[] = { G_GraphicsQueueFamilyIndex.value(), G_PresentQueueFamilyIndex.value()};
			const vk::SwapchainCreateInfoKHR swapchainCI = vk::SwapchainCreateInfoKHR(
				{},
				G_Surface,
				G_SurfaceImageCount,
				G_SurfaceFormat.format,
				G_SurfaceFormat.colorSpace,
				G_SwapchainExtent,
				1,
				vk::ImageUsageFlagBits::eColorAttachment,
				vk::SharingMode::eConcurrent,
				2,
				QueueFamilyIndices,
				SurfaceCapabilities.currentTransform,
				vk::CompositeAlphaFlagBitsKHR::eOpaque,
				G_SurfacePresentMode,
				vk::True
				);
			G_Swapchain = G_Device.createSwapchainKHR(swapchainCI, nullptr, G_DLD);
		}
		else {
			const vk::SwapchainCreateInfoKHR swapchainCI = vk::SwapchainCreateInfoKHR(
				{},
				G_Surface,
				G_SurfaceImageCount,
				G_SurfaceFormat.format,
				G_SurfaceFormat.colorSpace,
				G_SwapchainExtent,
				1,
				vk::ImageUsageFlagBits::eColorAttachment,
				vk::SharingMode::eExclusive,
				{},
				{},
				SurfaceCapabilities.currentTransform,
				vk::CompositeAlphaFlagBitsKHR::eOpaque,
				G_SurfacePresentMode,
				vk::True
				);
			G_Swapchain = G_Device.createSwapchainKHR(swapchainCI, nullptr, G_DLD);
		}

		if (!G_Swapchain)return;
		G_SwapchainOK = true;

	} catch(...) {
		return;
	}

	G_SwapchainImages = G_Device.getSwapchainImagesKHR(G_Swapchain, G_DLD);
	if (G_SwapchainImages.empty()) {
		DestroySwapchain();
		return;
	}

	const std::size_t NumImages = G_SwapchainImages.size();
	G_SwapchainImageViews.resize(NumImages);
	std::fill(G_SwapchainImageViews.begin(), G_SwapchainImageViews.end(), nullptr);

	for (std::size_t i = 0; i < NumImages; ++i) {
		const vk::ImageViewCreateInfo ImageViewCI = vk::ImageViewCreateInfo(
			{},
			G_SwapchainImages[i],
			vk::ImageViewType::e2D,
			G_SurfaceFormat.format,
			vk::ComponentMapping(),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
			);

		G_SwapchainImageViews[i] = G_Device.createImageView(ImageViewCI, nullptr, G_DLD);
		if (!G_SwapchainImageViews[i]) {
			DestroySwapchain();
			return;
		}
	}

	G_ColorBufferImages.resize(NumImages);
	G_ColorBufferDeviceMemories.resize(NumImages);
	G_ColorBufferImageViews.resize(NumImages);
	std::fill(G_ColorBufferImages.begin(), G_ColorBufferImages.end(), nullptr);
	std::fill(G_ColorBufferDeviceMemories.begin(), G_ColorBufferDeviceMemories.end(), nullptr);
	std::fill(G_ColorBufferImageViews.begin(), G_ColorBufferImageViews.end(), nullptr);

	for (std::size_t i = 0; i < NumImages; i++) {

		const vk::ImageCreateInfo ImageCI = vk::ImageCreateInfo{
			{},
			vk::ImageType::e2D,
			G_SurfaceFormat.format,
			vk::Extent3D{G_SwapchainExtent.width, G_SwapchainExtent.height, 1},
			1,
			1,
			G_SampleCount,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment,
			vk::SharingMode::eExclusive,
			{},
			vk::ImageLayout::eUndefined
		};

		G_ColorBufferImages[i] = G_Device.createImage(ImageCI, nullptr, G_DLD);
		if (!G_ColorBufferImages[i]) {
			DestroySwapchain();
			return;
		}

		const vk::MemoryRequirements ImageMemReqs = G_Device.getImageMemoryRequirements(G_ColorBufferImages[i], G_DLD);
		const std::uint32_t ImageMemoryTypeIndex = FindMemoryTypeIndex(ImageMemReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		const vk::MemoryAllocateInfo ImageMemoryAI = vk::MemoryAllocateInfo{ImageMemReqs.size, ImageMemoryTypeIndex};

		G_ColorBufferDeviceMemories[i] = G_Device.allocateMemory(ImageMemoryAI, nullptr, G_DLD);
		if (!G_ColorBufferDeviceMemories[i]) {
			DestroySwapchain();
			return;
		}

		G_Device.bindImageMemory(G_ColorBufferImages[i], G_ColorBufferDeviceMemories[i], 0, G_DLD);

		const vk::ImageViewCreateInfo ImageViewCI = vk::ImageViewCreateInfo{
			{},
			G_ColorBufferImages[i],
			vk::ImageViewType::e2D,
			G_SurfaceFormat.format,
			{},
			vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		};

		G_ColorBufferImageViews[i] = G_Device.createImageView(ImageViewCI, nullptr, G_DLD);
		if (!G_ColorBufferImageViews[i]) {
			DestroySwapchain();
			return;
		}
	}

	G_Framebuffers.resize(NumImages);
	std::fill(G_Framebuffers.begin(), G_Framebuffers.end(), nullptr);

	for (std::size_t i = 0; i < NumImages; ++i) {
		const std::vector<vk::ImageView> Attachments = {
			G_ColorBufferImageViews[i],
			G_SwapchainImageViews[i],
		};

		const vk::FramebufferCreateInfo FramebufferCI = vk::FramebufferCreateInfo(
			{},
			G_RenderPass,
			static_cast<std::uint32_t>(Attachments.size()),
			Attachments.data(),
			G_SwapchainExtent.width,
			G_SwapchainExtent.height,
			1
			);

		G_Framebuffers[i] = G_Device.createFramebuffer(FramebufferCI, nullptr, G_DLD);
		if (!G_Framebuffers[i]) {
			DestroySwapchain();
			return;
		}
	}

}

void DestroySwapchain()
{
	G_SwapchainOK = false;

	if (G_Device) {
		G_Device.waitIdle(G_DLD);

		for(auto& Item : G_Framebuffers) {
			if (Item) {
				G_Device.destroyFramebuffer(Item, nullptr, G_DLD);
				Item = nullptr;
			}
			G_Framebuffers.clear();
		}

		for(auto& Item : G_ColorBufferImageViews) {
			if (Item) {
				G_Device.destroyImageView(Item, nullptr, G_DLD);
				Item = nullptr;
			}
			G_ColorBufferImageViews.clear();
		}
		for(auto& Item : G_ColorBufferDeviceMemories) {
			if (Item) {
				G_Device.freeMemory(Item, nullptr, G_DLD);
				Item = nullptr;
			}
			G_ColorBufferDeviceMemories.clear();
		}
		for(auto& Item : G_ColorBufferImages) {
			if (Item) {
				G_Device.destroyImage(Item, nullptr, G_DLD);
				Item = nullptr;
			}
			G_ColorBufferImages.clear();
		}

		for(auto& Item : G_SwapchainImageViews) {
			if (Item) {
				G_Device.destroyImageView(Item, nullptr, G_DLD);
				Item = nullptr;
			}
			G_SwapchainImageViews.clear();
		}

		G_SwapchainImages.clear();

		if (G_Swapchain) {
			G_Device.destroySwapchainKHR(G_Swapchain, nullptr, G_DLD);
			G_Swapchain = nullptr;
		}
	}
}

void RecreateSwapchain()
{
	DestroySwapchain();
	CreateSwapchain();
}

void ShutdownSwapchain()
{
	DestroySwapchain();
}





