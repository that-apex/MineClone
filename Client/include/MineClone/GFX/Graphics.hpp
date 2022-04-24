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
}; // class QueueFamilyIndices

class VulkanContext
{
  public:
    NON_COPYTABLE(VulkanContext);
    NON_MOVABLE(VulkanContext);

    VulkanContext() = default;
    ~VulkanContext();

  public:
    void Initialize(GLFWwindow *window);

    void Destroy();

    [[nodiscard]] std::vector<VkExtensionProperties> &GetExtensions() noexcept;
    [[nodiscard]] VkInstance GetInstance() noexcept;
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() noexcept;
    [[nodiscard]] VkDevice GetDevice() noexcept;
    [[nodiscard]] VkQueue GetGraphicsQueue() noexcept;

  private:
    void CreateInstance();
    void SetupDebugCallbacks();
    void CreateSurface(GLFWwindow *window);
    void PickPhysicalDevice();
    void CreateLogicalDevice();

  private:
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

}; // class VulkanInitializer

} // namespace MineClone

#endif // MINECLONE_CLIENT_GFX_GRAPHICS_HPP_
