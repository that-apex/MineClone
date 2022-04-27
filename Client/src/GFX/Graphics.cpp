#include <MineClone/GFX/Graphics.hpp>

#include <MineClone/GFX/Shader.hpp>
#include <MineClone/Utility.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <set>

#include <MineClone_Client_Shaders.hpp>

#ifndef NDEBUG
#define ENABLE_VALIDATION_LAYERS
#endif

namespace MineClone
{

namespace
{

template <typename T, typename Func, typename... Args> std::vector<T> VulkanEnumerate(Func func, Args &&...args)
{
    uint32_t count;
    func(std::forward<Args>(args)..., &count, nullptr);
    std::vector<T> data{count};
    func(std::forward<Args>(args)..., &count, data.data());
    return data;
}

} // namespace

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
    CreateImageViews();
    CreateRenderPass();
    CreateGraphicsPipeline();
    CreateFramebuffers();
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
    vkAcquireNextImageKHR(m_device, m_swapChain, std::numeric_limits<uint64_t>::max(), frameData.ImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

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
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frameData.RenderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapChain;
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

namespace
{

bool IsFormatBest(const VkSurfaceFormatKHR &format)
{
    return format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
}

bool IsPresentModeBest(const VkPresentModeKHR &mode)
{
    return mode == VK_PRESENT_MODE_MAILBOX_KHR;
}

} // namespace

void VulkanContext::CreateSwapChain()
{
    // pick format and present modes
    m_swapChainFormat = FirstOrDefault(m_swapChainSupportDetails.Formats, IsFormatBest, m_swapChainSupportDetails.Formats[0]);
    m_swapChainPresentMode = FirstOrDefault(m_swapChainSupportDetails.PresentModes, IsPresentModeBest, VK_PRESENT_MODE_FIFO_KHR);

    // create swap chain extent
    const VkSurfaceCapabilitiesKHR &cap = m_swapChainSupportDetails.Capabilities;

    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    m_swapChainExtent.width = std::clamp<uint32_t>(width, cap.minImageExtent.width, cap.maxImageExtent.width);
    m_swapChainExtent.height = std::clamp<uint32_t>(height, cap.minImageExtent.height, cap.maxImageExtent.height);

    // create the swapchain
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = cap.minImageCount + 1;
    createInfo.imageFormat = m_swapChainFormat.format;
    createInfo.imageColorSpace = m_swapChainFormat.colorSpace;
    createInfo.imageExtent = m_swapChainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // check to make sure we're not requesting more images than the device can support
    if (cap.maxImageCount > 0 && cap.maxImageCount < createInfo.minImageCount)
        createInfo.minImageCount = cap.maxImageCount;

    if (m_queueFamilyIndices.GraphicsFamily == m_queueFamilyIndices.PresentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    else
    {
        const uint32_t indices[] = {*m_queueFamilyIndices.GraphicsFamily, *m_queueFamilyIndices.PresentFamily};
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = indices;
    }

    createInfo.preTransform = cap.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = m_swapChainPresentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
        throw GraphicsException("failed to create swap chain");

    m_swapChainImages = VulkanEnumerate<VkImage>(&vkGetSwapchainImagesKHR, m_device, m_swapChain);
}

void VulkanContext::CreateImageViews()
{
    m_swapChainImageViews.reserve(m_swapChainImages.size());

    for (const VkImage &image : m_swapChainImages)
    {
        VkImageViewCreateInfo createInfo{};

        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_swapChainFormat.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkImageView &view = m_swapChainImageViews.emplace_back();

        if (vkCreateImageView(m_device, &createInfo, nullptr, &view) != VK_SUCCESS)
            throw GraphicsException("failed to create an image view");
    }
}

void VulkanContext::CreateRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapChainFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
        throw GraphicsException("failed to create render pass");
}

void VulkanContext::CreateGraphicsPipeline()
{
    ShaderCompiler compiler;

    const CompiledShader compiledFragShader = compiler.Compile(RES_FRAGMENT_SHADER, "fragment.frag", shaderc_fragment_shader);
    const CompiledShader compiledVertShader = compiler.Compile(RES_VERTEX_SHADER, "vertex.vert", shaderc_vertex_shader);

    const ShaderModule fragShader{m_device, compiledFragShader};
    const ShaderModule vertShader{m_device, compiledVertShader};

    const std::array<VkPipelineShaderStageCreateInfo, 2> vertShaderStageInfo = {fragShader.CreateInfo(), vertShader.CreateInfo()};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_swapChainExtent.width;
    viewport.height = (float)m_swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // TODO: Dynamic states

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        throw GraphicsException("failed to create pipeline layout!");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(vertShaderStageInfo.size());
    pipelineInfo.pStages = vertShaderStageInfo.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
        throw GraphicsException("failed to create graphics pipeline!");
}

void VulkanContext::CreateFramebuffers()
{
    m_swapChainFramebuffers.reserve(m_swapChainImageViews.size());

    for (const VkImageView &view : m_swapChainImageViews)
    {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &view;
        framebufferInfo.width = m_swapChainExtent.width;
        framebufferInfo.height = m_swapChainExtent.height;
        framebufferInfo.layers = 1;

        VkFramebuffer &buffer = m_swapChainFramebuffers.emplace_back();
        if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &buffer) != VK_SUCCESS)
            throw GraphicsException("failed to create a framebuffer");
    }
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
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapChainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
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

    for (VkFramebuffer &item : m_swapChainFramebuffers)
        vkDestroyFramebuffer(m_device, item, nullptr);

    m_swapChainFramebuffers.clear();

    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    for (VkImageView &item : m_swapChainImageViews)
        vkDestroyImageView(m_device, item, nullptr);

    m_swapChainImageViews.clear();

    if (m_swapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
        m_swapChain = VK_NULL_HANDLE;
    }

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

VkQueue VulkanContext::GetPresentQueue() noexcept
{
    return m_presentQueue;
}

VkSurfaceFormatKHR &VulkanContext::GetSwapChainFormat() noexcept
{
    return m_swapChainFormat;
}

VkExtent2D &VulkanContext::GetSwapChainExtent() noexcept
{
    return m_swapChainExtent;
}

VkSwapchainKHR VulkanContext::GetSwapChain() noexcept
{
    return m_swapChain;
}

std::vector<VkImage> &VulkanContext::GetSwapChainImages() noexcept
{
    return m_swapChainImages;
}

std::vector<VkImageView> &VulkanContext::GetSwapChainImageViews() noexcept
{
    return m_swapChainImageViews;
}

VkRenderPass VulkanContext::GetRenderPass() noexcept
{
    return m_renderPass;
}

VkPipelineLayout VulkanContext::GetPipelineLayout() noexcept
{
    return m_pipelineLayout;
}

VkPipeline VulkanContext::GetPipeline() noexcept
{
    return m_pipeline;
}

std::vector<VkFramebuffer> &VulkanContext::GetSwapChainFramebuffers() noexcept
{
    return m_swapChainFramebuffers;
}

VkCommandPool VulkanContext::GetCommandPool() noexcept
{
    return m_commandPool;
}

} // namespace MineClone
