#ifndef MINECLONE_CLIENT_GFX_SHADER_HPP_
#define MINECLONE_CLIENT_GFX_SHADER_HPP_

#include "Graphics.hpp"

#include <shaderc/shaderc.hpp>

namespace MineClone
{

class ShaderException : public Exception
{
  public:
    inline explicit ShaderException(std::string message) : Exception(std::move(message))
    {
    }
}; // class GraphicsException

struct CompiledShader
{
    std::string Name;
    shaderc_shader_kind Kind;
    std::vector<uint32_t> Data;
};

class ShaderCompiler
{
  public:
    CompiledShader Compile(const std::string &data, const std::string &name, shaderc_shader_kind kind);

  private:
    shaderc::Compiler m_compiler;
    shaderc::CompileOptions m_options;
}; // class ShaderCompiler

class ShaderModule
{
  public:
    ShaderModule(VkDevice device, const CompiledShader &compiledShader);
    virtual ~ShaderModule();

    NON_COPYABLE(ShaderModule);
    MOVEABLE(ShaderModule);

  public:
    void Destroy();

    [[nodiscard]] VkShaderModule GetVkModule() const noexcept;

    [[nodiscard]] shaderc_shader_kind GetKind() const noexcept;

    [[nodiscard]] VkPipelineShaderStageCreateInfo CreateInfo() const;

  private:
    VkDevice m_device;
    shaderc_shader_kind m_kind;
    VkShaderModule m_module{VK_NULL_HANDLE};
}; // class ShaderModule

} // namespace MineClone

#endif // MINECLONE_CLIENT_GFX_SHADER_HPP_
