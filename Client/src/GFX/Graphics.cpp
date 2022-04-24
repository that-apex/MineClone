#include <MineClone/GFX/Graphics.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <set>

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

VulkanContext::~VulkanContext()
{
    Destroy();
}

void VulkanContext::Initialize(GLFWwindow *window)
{
#ifdef ENABLE_VALIDATION_LAYERS
    m_requireValidationLayers = true;
#endif

    CreateInstance();

    if (m_requireValidationLayers)
        SetupDebugCallbacks();

    CreateSurface(window);
    PickPhysicalDevice();
    CreateLogicalDevice();
}

void VulkanContext::CreateInstance()
{
    // required extensions
    std::vector<const char *> requiredExtensions{};

    // setup validation layers
    if (m_requireValidationLayers)
    {
        m_requiredValidationLayers.emplace_back("VK_LAYER_KHRONOS_validation");

        const std::vector<VkLayerProperties> validationLayers =
            VulkanEnumerate<VkLayerProperties>(&vkEnumerateInstanceLayerProperties);

        for (const char *required : m_requiredValidationLayers)
        {
            if (std::none_of(
                    begin(validationLayers), end(validationLayers),
                    [required](const VkLayerProperties &layer) { return std::strcmp(layer.layerName, required) == 0; }))
            {
                throw GraphicsException("Missing validation layer: "s + required);
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
    createInfo.enabledExtensionCount = static_cast<uint32_t >(requiredExtensions.size());
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

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                             VkDebugUtilsMessageTypeFlagsEXT messageType,
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

    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    createInfo.pfnUserCallback = &debugCallback;
    createInfo.pUserData = nullptr;

    auto vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

    ASSERT(vkCreateDebugUtilsMessengerEXT, "vkCreateDebugUtilsMessengerEXT not present");

    if (vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
        throw GraphicsException("failed to set up debug callback");
}

void VulkanContext::CreateSurface(GLFWwindow *window)
{
    if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS)
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

struct ScoredGPU
{
    VkPhysicalDevice Device;
    QueueFamilyIndices Indices;
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
        if (std::none_of(begin(presentExtensions), end(presentExtensions), [&](const VkExtensionProperties &extension) {
                return strcmp(extension.extensionName, requiredExtension) == 0;
            }))
        {
            scored.Score = INADEQUATE_GPU_SCORE;
            return scored;
        }
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
    const std::vector<VkPhysicalDevice> allDevices =
        VulkanEnumerate<VkPhysicalDevice>(&vkEnumeratePhysicalDevices, m_instance);

    // score devices and pick the best one
    std::vector<ScoredGPU> scoredDevices;
    std::transform(begin(allDevices), end(allDevices), std::back_inserter(scoredDevices),
                   [&](const VkPhysicalDevice &device) { return ScorePhysicalGPU(device, m_surface); });

    const auto bestDevice = std::max_element(begin(scoredDevices), end(scoredDevices));

    if (bestDevice == end(scoredDevices) || bestDevice->Score == INADEQUATE_GPU_SCORE)
        throw GraphicsException("no suitable physical devices found");

    m_physicalDevice = bestDevice->Device;
    m_queueFamilyIndices = bestDevice->Indices;
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
    createInfo.queueCreateInfoCount = static_cast<uint32_t >(queueCreateInfos.size());
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

void VulkanContext::Destroy()
{
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
        auto vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));

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

} // namespace MineClone
