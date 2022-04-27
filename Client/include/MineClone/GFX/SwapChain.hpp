#pragma once
#ifndef MINECLONE_CLIENT_GFX_SWAPCHAIN_HPP_
#define MINECLONE_CLIENT_GFX_SWAPCHAIN_HPP_

#include "Graphics.hpp"

namespace MineClone
{

class VulkanContext; // Graphics.hpp

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

class SwapChain
{
  public:
    NON_COPYABLE(SwapChain);
    NON_MOVABLE(SwapChain);

    SwapChain() = default;

    ~SwapChain();

    void Create(VulkanContext *context);

    void Destroy();

    [[nodiscard]] VkSurfaceFormatKHR &GetSwapChainFormat() noexcept;
    [[nodiscard]] VkExtent2D &GetSwapChainExtent() noexcept;
    [[nodiscard]] VkSwapchainKHR GetSwapChain() noexcept;
    [[nodiscard]] std::vector<VkImage> &GetSwapChainImages() noexcept;
    [[nodiscard]] std::vector<VkImageView> &GetSwapChainImageViews() noexcept;
    [[nodiscard]] VkRenderPass GetRenderPass() noexcept;
    [[nodiscard]] VkPipelineLayout GetPipelineLayout() noexcept;
    [[nodiscard]] VkPipeline GetPipeline() noexcept;
    [[nodiscard]] std::vector<VkFramebuffer> &GetSwapChainFramebuffers() noexcept;

  private:
    void CreateSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    void CreateFramebuffers();

  private:
    VulkanContext *m_context{nullptr};
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
}; // class SwapChain

} // namespace MineClone

#endif // MINECLONE_CLIENT_GFX_SWAPCHAIN_HPP_
