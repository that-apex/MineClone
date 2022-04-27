#include <MineClone/GFX/SwapChain.hpp>

#include <algorithm>

#include <MineClone/GFX/Shader.hpp>
#include <MineClone/GFX/VulkanContext.hpp>
#include <MineClone/Utility.hpp>

#include <MineClone_Client_Shaders.hpp>

namespace MineClone
{

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

SwapChain::~SwapChain()
{
    Destroy();
}

void SwapChain::Create(VulkanContext *context)
{
    Destroy();
    m_context = context;

    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    CreateGraphicsPipeline();
    CreateFramebuffers();
}

void SwapChain::CreateSwapChain()
{
    const SwapChainSupportDetails& supportDetails = m_context->GetSwapChainSupportDetails();

    // pick format and present modes
    m_swapChainFormat = FirstOrDefault(supportDetails.Formats, IsFormatBest, supportDetails.Formats[0]);
    m_swapChainPresentMode = FirstOrDefault(supportDetails.PresentModes, IsPresentModeBest, VK_PRESENT_MODE_FIFO_KHR);

    // create swap chain extent
    const VkSurfaceCapabilitiesKHR &cap = supportDetails.Capabilities;

    int width, height;
    glfwGetFramebufferSize(m_context->GetWindow(), &width, &height);

    m_swapChainExtent.width = std::clamp<uint32_t>(width, cap.minImageExtent.width, cap.maxImageExtent.width);
    m_swapChainExtent.height = std::clamp<uint32_t>(height, cap.minImageExtent.height, cap.maxImageExtent.height);

    // create the swapchain
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_context->GetSurface();
    createInfo.minImageCount = cap.minImageCount + 1;
    createInfo.imageFormat = m_swapChainFormat.format;
    createInfo.imageColorSpace = m_swapChainFormat.colorSpace;
    createInfo.imageExtent = m_swapChainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // check to make sure we're not requesting more images than the device can support
    if (cap.maxImageCount > 0 && cap.maxImageCount < createInfo.minImageCount)
        createInfo.minImageCount = cap.maxImageCount;

    if (m_context->GetQueueFamilyIndices().GraphicsFamily == m_context->GetQueueFamilyIndices().PresentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    else
    {
        const uint32_t indices[] = {*m_context->GetQueueFamilyIndices().GraphicsFamily, *m_context->GetQueueFamilyIndices().PresentFamily};
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = indices;
    }

    createInfo.preTransform = cap.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = m_swapChainPresentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_context->GetDevice(), &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
        throw GraphicsException("failed to create swap chain");

    m_swapChainImages = VulkanEnumerate<VkImage>(&vkGetSwapchainImagesKHR, m_context->GetDevice(), m_swapChain);
}

void SwapChain::CreateImageViews()
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

        if (vkCreateImageView(m_context->GetDevice(), &createInfo, nullptr, &view) != VK_SUCCESS)
            throw GraphicsException("failed to create an image view");
    }
}

void SwapChain::CreateRenderPass()
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

    if (vkCreateRenderPass(m_context->GetDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
        throw GraphicsException("failed to create render pass");
}

void SwapChain::CreateGraphicsPipeline()
{
    ShaderCompiler compiler;

    const CompiledShader compiledFragShader = compiler.Compile(RES_FRAGMENT_SHADER, "fragment.frag", shaderc_fragment_shader);
    const CompiledShader compiledVertShader = compiler.Compile(RES_VERTEX_SHADER, "vertex.vert", shaderc_vertex_shader);

    const ShaderModule fragShader{m_context->GetDevice(), compiledFragShader};
    const ShaderModule vertShader{m_context->GetDevice(), compiledVertShader};

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

    if (vkCreatePipelineLayout(m_context->GetDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
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

    if (vkCreateGraphicsPipelines(m_context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
        throw GraphicsException("failed to create graphics pipeline!");
}

void SwapChain::CreateFramebuffers()
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
        if (vkCreateFramebuffer(m_context->GetDevice(), &framebufferInfo, nullptr, &buffer) != VK_SUCCESS)
            throw GraphicsException("failed to create a framebuffer");
    }
}


void SwapChain::Destroy()
{
    if (m_context == nullptr)
        return;

    for (VkFramebuffer &item : m_swapChainFramebuffers)
        vkDestroyFramebuffer(m_context->GetDevice(), item, nullptr);

    m_swapChainFramebuffers.clear();

    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_context->GetDevice(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_context->GetDevice(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_context->GetDevice(), m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    for (VkImageView &item : m_swapChainImageViews)
        vkDestroyImageView(m_context->GetDevice(), item, nullptr);

    m_swapChainImageViews.clear();

    if (m_swapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_context->GetDevice(), m_swapChain, nullptr);
        m_swapChain = VK_NULL_HANDLE;
    }
}

VkSurfaceFormatKHR &SwapChain::GetSwapChainFormat() noexcept
{
    return m_swapChainFormat;
}

VkExtent2D &SwapChain::GetSwapChainExtent() noexcept
{
    return m_swapChainExtent;
}

VkSwapchainKHR SwapChain::GetSwapChain() noexcept
{
    return m_swapChain;
}

std::vector<VkImage> &SwapChain::GetSwapChainImages() noexcept
{
    return m_swapChainImages;
}

std::vector<VkImageView> &SwapChain::GetSwapChainImageViews() noexcept
{
    return m_swapChainImageViews;
}

VkRenderPass SwapChain::GetRenderPass() noexcept
{
    return m_renderPass;
}

VkPipelineLayout SwapChain::GetPipelineLayout() noexcept
{
    return m_pipelineLayout;
}

VkPipeline SwapChain::GetPipeline() noexcept
{
    return m_pipeline;
}

std::vector<VkFramebuffer> &SwapChain::GetSwapChainFramebuffers() noexcept
{
    return m_swapChainFramebuffers;
}

} // namespace MineClone