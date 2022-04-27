#include <MineClone/GFX/VulkanContext.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <set>

#ifndef NDEBUG
#define ENABLE_VALIDATION_LAYERS
#endif

namespace MineClone
{
InFlightFrameData::~InFlightFrameData()
{
    Destroy();
}

void InFlightFrameData::Initialize(VulkanContext *context)
{
    Context = context;
}

void InFlightFrameData::Destroy()
{
    if (Context == nullptr)
        return;

    if (InFlightFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(Context->GetDevice(), InFlightFence, nullptr);
        InFlightFence = VK_NULL_HANDLE;
    }

    if (RenderFinishedSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(Context->GetDevice(), RenderFinishedSemaphore, nullptr);
        RenderFinishedSemaphore = VK_NULL_HANDLE;
    }

    if (ImageAvailableSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(Context->GetDevice(), ImageAvailableSemaphore, nullptr);
        ImageAvailableSemaphore = VK_NULL_HANDLE;
    }

    if (CommandBuffer != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(Context->GetDevice(), Context->GetCommandPool(), 1, &CommandBuffer);
        CommandBuffer = VK_NULL_HANDLE;
    }
}

VulkanContext::VulkanContext()
{
    for (InFlightFrameData &frameData : m_inFlightFrameData)
        frameData.Initialize(this);
}

VulkanContext::~VulkanContext()
{
    Destroy();
}

void VulkanContext::Initialize(GLFWwindow *window)
{
#ifdef ENABLE_VALIDATION_LAYERS
    m_requireValidationLayers = true;
#endif

    m_window = window;

    CreateInstance();

    if (m_requireValidationLayers)
        SetupDebugCallbacks();

    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateCommandPool();
    CreateCommandBuffer();
    CreateSyncObjects();
}

void VulkanContext::Render()
{
    InFlightFrameData &frameData = m_inFlightFrameData[m_currentFrame];

    // wait for previous frame
    vkWaitForFences(m_device, 1, &frameData.InFlightFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    vkResetFences(m_device, 1, &frameData.InFlightFence);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(m_device, m_swapChain.GetSwapChain(), std::numeric_limits<uint64_t>::max(), frameData.ImageAvailableSemaphore,
                          VK_NULL_HANDLE, &imageIndex);

    // record framebuffer
    vkResetCommandBuffer(frameData.CommandBuffer, 0);
    RecordCommandBuffer(frameData.CommandBuffer, imageIndex);

    // submit framebuffer
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frameData.ImageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frameData.CommandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frameData.RenderFinishedSemaphore;

    if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, frameData.InFlightFence) != VK_SUCCESS)
        throw std::runtime_error("failed to submit draw command buffer!");

    VkPresentInfoKHR presentInfo{};
    VkSwapchainKHR swapChain = m_swapChain.GetSwapChain();
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frameData.RenderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &imageIndex;

    if (vkQueuePresentKHR(m_presentQueue, &presentInfo) != VK_SUCCESS)
        throw std::runtime_error("failed to present swap chain image!");

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanContext::CreateInstance()
{
    const std::vector<const char *> optionalValidationLayers = {"VK_LAYER_LUNARG_monitor"};

    // required extensions
    std::vector<const char *> requiredExtensions{};

    // setup validation layers
    if (m_requireValidationLayers)
    {
        m_requiredValidationLayers.emplace_back("VK_LAYER_KHRONOS_validation");

        const std::vector<VkLayerProperties> validationLayers = VulkanEnumerate<VkLayerProperties>(&vkEnumerateInstanceLayerProperties);

        for (const char *required : m_requiredValidationLayers)
        {
            if (std::none_of(begin(validationLayers), end(validationLayers),
                             [required](const VkLayerProperties &layer) { return std::strcmp(layer.layerName, required) == 0; }))
            {
                throw GraphicsException("Missing validation layer: "s + required);
            }
        }

        for (const char *optional : optionalValidationLayers)
        {
            if (std::any_of(begin(validationLayers), end(validationLayers),
                            [optional](const VkLayerProperties &layer) { return std::strcmp(layer.layerName, optional) == 0; }))
            {
                m_requiredValidationLayers.emplace_back(optional);
            }
        }

        requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // query for glfw extension
    uint32_t extensionCount = 0;
    const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    for (uint32_t i = 0; i < extensionCount; i++)
        requiredExtensions.emplace_back(extensions[i]);

    // create instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Not Minecraft";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(m_requiredValidationLayers.size());
    createInfo.ppEnabledLayerNames = m_requiredValidationLayers.data();

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
        throw GraphicsException("vkCreateInstance failed");

    // query for vulkan extension
    m_extensions = VulkanEnumerate<VkExtensionProperties>(&vkEnumerateInstanceExtensionProperties, nullptr);
}

namespace
{

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
                                             const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

} // namespace

void VulkanContext::SetupDebugCallbacks()
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    createInfo.pfnUserCallback = &debugCallback;
    createInfo.pUserData = nullptr;

    auto vkCreateDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

    ASSERT(vkCreateDebugUtilsMessengerEXT, "vkCreateDebugUtilsMessengerEXT not present");

    if (vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
        throw GraphicsException("failed to set up debug callback");
}

void VulkanContext::CreateSurface()
{
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
        throw GraphicsException("failed to create window surface");
}

namespace
{

constexpr const uint32_t INADEQUATE_GPU_SCORE = 0;
const std::array<const char *, 1> REQUIRED_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices = {};

    const std::vector<VkQueueFamilyProperties> queueFamilyProperties =
        VulkanEnumerate<VkQueueFamilyProperties>(&vkGetPhysicalDeviceQueueFamilyProperties, device);

    for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i)
    {
        const auto &current = queueFamilyProperties[i];

        VkBool32 canPresent = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &canPresent);

        if (current.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.GraphicsFamily = i;

        if (canPresent == VK_TRUE)
            indices.PresentFamily = i;

        if (indices.IsComplete())
            break;
    }

    return indices;
}

SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.Capabilities);

    details.Formats = VulkanEnumerate<VkSurfaceFormatKHR>(&vkGetPhysicalDeviceSurfaceFormatsKHR, device, surface);
    details.PresentModes = VulkanEnumerate<VkPresentModeKHR>(&vkGetPhysicalDeviceSurfacePresentModesKHR, device, surface);

    return details;
}

struct ScoredGPU
{
    VkPhysicalDevice Device;
    QueueFamilyIndices Indices;
    SwapChainSupportDetails SwapChainSupport;
    uint32_t Score;

    [[nodiscard]] bool operator<(const ScoredGPU &other) const
    {
        return (Score < other.Score);
    }
};

ScoredGPU ScorePhysicalGPU(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    ScoredGPU scored{};
    scored.Device = device;
    scored.Indices = FindQueueFamilies(device, surface);
    scored.Score = 0;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(device, &properties);
    vkGetPhysicalDeviceFeatures(device, &features);

    // disqualify gpu if it doesn't support all required extensions
    const std::vector<VkExtensionProperties> presentExtensions =
        VulkanEnumerate<VkExtensionProperties>(&vkEnumerateDeviceExtensionProperties, device, nullptr);

    for (const char *requiredExtension : REQUIRED_EXTENSIONS)
    {
        if (std::none_of(begin(presentExtensions), end(presentExtensions),
                         [&](const VkExtensionProperties &extension) { return strcmp(extension.extensionName, requiredExtension) == 0; }))
        {
            scored.Score = INADEQUATE_GPU_SCORE;
            return scored;
        }
    }

    // disqualify if swapchain is not adequate
    scored.SwapChainSupport = QuerySwapChainSupport(device, surface);

    if (!scored.SwapChainSupport.IsAdequate())
    {
        scored.Score = INADEQUATE_GPU_SCORE;
        return scored;
    }

    // disqualify gpu if it doesn't support required features
    if (!features.geometryShader || !scored.Indices.IsComplete())
    {
        scored.Score = INADEQUATE_GPU_SCORE;
        return scored;
    }

    // prefer discrete gpus
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        scored.Score += 1000;

    // prefer higher image dimensions
    scored.Score += properties.limits.maxImageDimension2D;

    return scored;
}

} // namespace

void VulkanContext::PickPhysicalDevice()
{
    const std::vector<VkPhysicalDevice> allDevices = VulkanEnumerate<VkPhysicalDevice>(&vkEnumeratePhysicalDevices, m_instance);

    // score devices and pick the best one
    std::vector<ScoredGPU> scoredDevices;
    std::transform(begin(allDevices), end(allDevices), std::back_inserter(scoredDevices),
                   [&](const VkPhysicalDevice &device) { return ScorePhysicalGPU(device, m_surface); });

    const auto bestDevice = std::max_element(begin(scoredDevices), end(scoredDevices));

    if (bestDevice == end(scoredDevices) || bestDevice->Score == INADEQUATE_GPU_SCORE)
        throw GraphicsException("no suitable physical devices found");

    m_physicalDevice = bestDevice->Device;
    m_queueFamilyIndices = bestDevice->Indices;
    m_swapChainSupportDetails = bestDevice->SwapChainSupport;
}

void VulkanContext::CreateLogicalDevice()
{
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &features);

    std::set<uint32_t> uniqueQueueIds{*m_queueFamilyIndices.GraphicsFamily, *m_queueFamilyIndices.PresentFamily};
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

    const float queuePriority = 1.0f;

    for (const auto &uniqueId : uniqueQueueIds)
    {
        VkDeviceQueueCreateInfo &info = queueCreateInfos.emplace_back();
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex = *m_queueFamilyIndices.GraphicsFamily;
        info.queueCount = 1;
        info.pQueuePriorities = &queuePriority;
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledLayerCount = static_cast<uint32_t>(m_requiredValidationLayers.size());
    createInfo.ppEnabledLayerNames = m_requiredValidationLayers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(REQUIRED_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = REQUIRED_EXTENSIONS.data();

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
        throw GraphicsException("failed to create a logical device");

    vkGetDeviceQueue(m_device, *m_queueFamilyIndices.GraphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, *m_queueFamilyIndices.PresentFamily, 0, &m_presentQueue);
}

void VulkanContext::CreateSwapChain()
{
    vkDeviceWaitIdle(m_device);
    m_swapChain.Create(this);
}

void VulkanContext::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = *m_queueFamilyIndices.GraphicsFamily;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
        throw GraphicsException("failed to create command pool!");
}

void VulkanContext::CreateCommandBuffer()
{

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    std::vector<VkCommandBuffer> buffers;
    buffers.resize(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateCommandBuffers(m_device, &allocInfo, buffers.data()) != VK_SUCCESS)
        throw GraphicsException("failed to allocate command buffers!");

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        m_inFlightFrameData[i].CommandBuffer = buffers[i];
}

void VulkanContext::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        throw GraphicsException("failed to begin recording command buffer!");

    VkClearValue clearColor{{{0.0f, 0.0f, 0.0f, 1.0f}}};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_swapChain.GetRenderPass();
    renderPassInfo.framebuffer = m_swapChain.GetSwapChainFramebuffers()[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapChain.GetSwapChainExtent();
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_swapChain.GetPipeline());
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        throw GraphicsException("failed to record command buffer!");
}

void VulkanContext::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (InFlightFrameData &data : m_inFlightFrameData)
    {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &data.ImageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &data.RenderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &data.InFlightFence) != VK_SUCCESS)
        {
            throw GraphicsException("failed to create synchronization objects for a frame!");
        }
    }
}

void VulkanContext::Destroy()
{
    if (m_device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_device);

    for (InFlightFrameData &data : m_inFlightFrameData)
        data.Destroy();

    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    m_swapChain.Destroy();

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_debugMessenger != VK_NULL_HANDLE)
    {
        auto vkDestroyDebugUtilsMessengerEXT =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));

        ASSERT(vkDestroyDebugUtilsMessengerEXT, "vkDestroyDebugUtilsMessengerEXT not present");

        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

GLFWwindow *VulkanContext::GetWindow() noexcept
{
    return m_window;
}

std::vector<VkExtensionProperties> &VulkanContext::GetExtensions() noexcept
{
    return m_extensions;
}

VkInstance VulkanContext::GetInstance() noexcept
{
    return m_instance;
}

VkPhysicalDevice VulkanContext::GetPhysicalDevice() noexcept
{
    return m_physicalDevice;
}

VkDevice VulkanContext::GetDevice() noexcept
{
    return m_device;
}

VkQueue VulkanContext::GetGraphicsQueue() noexcept
{
    return m_graphicsQueue;
}

VkQueue VulkanContext::GetPresentQueue() noexcept
{
    return m_presentQueue;
}

VkSurfaceKHR VulkanContext::GetSurface() noexcept
{
    return m_surface;
}

QueueFamilyIndices &VulkanContext::GetQueueFamilyIndices() noexcept
{
    return m_queueFamilyIndices;
}

SwapChainSupportDetails &VulkanContext::GetSwapChainSupportDetails() noexcept
{
    return m_swapChainSupportDetails;
}

SwapChain &VulkanContext::GetSwapChain() noexcept
{
    return m_swapChain;
}

VkCommandPool VulkanContext::GetCommandPool() noexcept
{
    return m_commandPool;
}

} // namespace MineClone
