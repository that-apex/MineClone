#pragma once
#ifndef MINECLONE_CLIENT_GFX_VULKANCONTEXT_HPP_
#define MINECLONE_CLIENT_GFX_VULKANCONTEXT_HPP_

#include <array>
#include <optional>
#include <vector>

#include "Graphics.hpp"
#include "SwapChain.hpp"

namespace MineClone
{

struct QueueFamilyIndices
{
    std::optional<uint32_t> GraphicsFamily;
    std::optional<uint32_t> PresentFamily;

    [[nodiscard]] inline bool IsComplete() const noexcept
    {
        return GraphicsFamily.has_value() && PresentFamily.has_value();
    }
}; // struct QueueFamilyIndices

class VulkanContext;

struct InFlightFrameData
{
  public:
    VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};
    VkSemaphore ImageAvailableSemaphore{VK_NULL_HANDLE};
    VkSemaphore RenderFinishedSemaphore{VK_NULL_HANDLE};
    VkFence InFlightFence{VK_NULL_HANDLE};

  private:
    VulkanContext *Context{nullptr};

  public:
    NON_COPYABLE(InFlightFrameData);
    NON_MOVABLE(InFlightFrameData);

    InFlightFrameData() = default;
    ~InFlightFrameData();

    void Initialize(VulkanContext *context);

    void Destroy();

}; // struct InFlightFrameData

class VulkanContext
{
  public:
    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

  public:
    NON_COPYABLE(VulkanContext);
    NON_MOVABLE(VulkanContext);

    VulkanContext();
    ~VulkanContext();

  public:
    void Initialize(GLFWwindow *window);

    void Render();

    void Destroy();

    [[nodiscard]] GLFWwindow *GetWindow() noexcept;
    [[nodiscard]] std::vector<VkExtensionProperties> &GetExtensions() noexcept;
    [[nodiscard]] VkInstance GetInstance() noexcept;
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() noexcept;
    [[nodiscard]] VkDevice GetDevice() noexcept;
    [[nodiscard]] VkQueue GetGraphicsQueue() noexcept;
    [[nodiscard]] VkQueue GetPresentQueue() noexcept;
    [[nodiscard]] VkSurfaceKHR GetSurface() noexcept;
    [[nodiscard]] QueueFamilyIndices &GetQueueFamilyIndices() noexcept;
    [[nodiscard]] SwapChainSupportDetails &GetSwapChainSupportDetails() noexcept;
    [[nodiscard]] SwapChain &GetSwapChain() noexcept;
    [[nodiscard]] VkCommandPool GetCommandPool() noexcept;

  private:
    void CreateInstance();
    void SetupDebugCallbacks();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapChain();
    void CreateCommandPool();
    void CreateCommandBuffer();
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void CreateSyncObjects();

  private:
    GLFWwindow *m_window{nullptr};
    bool m_requireValidationLayers{false};
    std::vector<const char *> m_requiredValidationLayers{};
    std::vector<VkExtensionProperties> m_extensions{};
    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    QueueFamilyIndices m_queueFamilyIndices{};
    VkDevice m_device{VK_NULL_HANDLE};
    VkQueue m_graphicsQueue{VK_NULL_HANDLE};
    VkQueue m_presentQueue{VK_NULL_HANDLE};
    SwapChainSupportDetails m_swapChainSupportDetails{};
    SwapChain m_swapChain{};
    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    std::array<InFlightFrameData, MAX_FRAMES_IN_FLIGHT> m_inFlightFrameData;

    size_t m_currentFrame{0};
}; // class VulkanInitializer

} // namespace MineClone

#endif // MINECLONE_CLIENT_GFX_VULKANCONTEXT_HPP_
