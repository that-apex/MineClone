#include <MineClone/GFX/Shader.hpp>

#include <algorithm>

#ifndef NDEBUG
#define OPTIMIZE_SHADERS
#endif

namespace MineClone
{

CompiledShader ShaderCompiler::Compile(const std::string &data, const std::string &name, shaderc_shader_kind kind)
{
#ifdef OPTIMIZE_SHADERS
    m_options.SetOptimizationLevel(shaderc_optimization_level_performance);
#endif

    shaderc::SpvCompilationResult result = m_compiler.CompileGlslToSpv(data, kind, name.c_str(), m_options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        throw ShaderException(result.GetErrorMessage());

    return CompiledShader{name, kind, std::vector<uint32_t>{std::begin(result), std::end(result)}};
}

ShaderModule::ShaderModule(VkDevice device, const CompiledShader &compiledShader)
    : m_device{device}, m_kind{compiledShader.Kind}
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = compiledShader.Data.size() * sizeof(uint32_t);
    createInfo.pCode = compiledShader.Data.data();

    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &m_module) != VK_SUCCESS)
        throw ShaderException("Failed to create shader module");
}

ShaderModule::~ShaderModule()
{
    Destroy();
}

ShaderModule::ShaderModule(ShaderModule &&rhs) noexcept
    : m_device{rhs.m_device}, m_kind{rhs.m_kind}, m_module{rhs.m_module}
{
    rhs.m_module = VK_NULL_HANDLE;
}

ShaderModule &ShaderModule::operator=(ShaderModule &&rhs) noexcept
{
    m_device = rhs.m_device;
    m_kind = rhs.m_kind;
    m_module = rhs.m_module;
    rhs.m_module = VK_NULL_HANDLE;
    return *this;
}

void ShaderModule::Destroy()
{
    if (m_module != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_module, nullptr);
        m_module = VK_NULL_HANDLE;
    }
}

VkShaderModule ShaderModule::GetVkModule() const noexcept
{
    return m_module;
}

shaderc_shader_kind ShaderModule::GetKind() const noexcept
{
    return m_kind;
}

VkPipelineShaderStageCreateInfo ShaderModule::CreateInfo() const
{
    VkPipelineShaderStageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

    switch (GetKind())
    {
    case shaderc_fragment_shader:
        createInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case shaderc_vertex_shader:
        createInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    default:
        throw ShaderException("Unknown shader kind " + std::to_string(GetKind()));
    }

    createInfo.module = GetVkModule();
    createInfo.pName = "main";
    return createInfo;
}

} // namespace MineClone
