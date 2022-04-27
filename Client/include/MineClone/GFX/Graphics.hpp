#pragma once
#ifndef MINECLONE_CLIENT_GFX_GRAPHICS_HPP_
#define MINECLONE_CLIENT_GFX_GRAPHICS_HPP_

#include "../Common.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <optional>
#include <vector>

namespace MineClone
{

class GraphicsException : public Exception
{
  public:
    inline explicit GraphicsException(std::string message) : Exception(std::move(message))
    {
    }
}; // class GraphicsException

struct QueueFamilyIndices
{
    std::optional<uint32_t> GraphicsFamily;
    std::optional<uint32_t> PresentFamily;

    [[nodiscard]] inline bool IsComplete() const noexcept
    {
        return GraphicsFamily.has_value() && PresentFamily.has_value();
    }
}; // struct QueueFamilyIndices

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR Capabilities;
    std::vector<VkSurfaceFormatKHR> Formats;
    std::vector<VkPresentModeKHR> PresentModes;

    [[nodiscard]] inline bool IsAdequate() const noexcept
    {
        return !Formats.empty() && !PresentModes.empty();
    }
}; // struct SwapChainSupportDetails

class VulkanContext
{
  public:
    NON_COPYABLE(VulkanContext);
    NON_MOVABLE(VulkanContext);

    VulkanContext() = default;
    ~VulkanContext();

  public:
    void Initialize(GLFWwindow *window);

    void Render();

    void Destroy();

    [[nodiscard]] std::vector<VkExtensionProperties> &GetExtensions() noexcept;
    [[nodiscard]] VkInstance GetInstance() noexcept;
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() noexcept;
    [[nodiscard]] VkDevice GetDevice() noexcept;
    [[nodiscard]] VkQueue GetGraphicsQueue() noexcept;

  private:
    void CreateInstance();
    void SetupDebugCallbacks();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    void CreateFramebuffers();
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
    SwapChainSupportDetails m_swapChainSupportDetails{};
    VkDevice m_device{VK_NULL_HANDLE};
    VkQueue m_graphicsQueue{VK_NULL_HANDLE};
    VkQueue m_presentQueue{VK_NULL_HANDLE};
    VkSurfaceFormatKHR m_swapChainFormat{};
    VkPresentModeKHR m_swapChainPresentMode{};
    VkExtent2D m_swapChainExtent{};
    VkSwapchainKHR m_swapChain{VK_NULL_HANDLE};
    std::vector<VkImage> m_swapChainImages;
    std::vector<VkImageView> m_swapChainImageViews;
    VkRenderPass m_renderPass{VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> m_swapChainFramebuffers{};
    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
    VkSemaphore m_imageAvailableSemaphore{VK_NULL_HANDLE};
    VkSemaphore m_renderFinishedSemaphore{VK_NULL_HANDLE};
    VkFence m_inFlightFence{VK_NULL_HANDLE};

}; // class VulkanInitializer

} // namespace MineClone

#endif // MINECLONE_CLIENT_GFX_GRAPHICS_HPP_
