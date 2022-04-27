#pragma once
#ifndef MINECLONE_CLIENT_GFX_GRAPHICS_HPP_
#define MINECLONE_CLIENT_GFX_GRAPHICS_HPP_

#include "../Common.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

namespace MineClone
{

class GraphicsException : public Exception
{
  public:
    explicit GraphicsException(std::string message);

}; // class GraphicsException

template <typename T, typename Func, typename... Args> inline std::vector<T> VulkanEnumerate(Func func, Args &&...args)
{
    uint32_t count;
    func(std::forward<Args>(args)..., &count, nullptr);
    std::vector<T> data{count};
    func(std::forward<Args>(args)..., &count, data.data());
    return data;
}

} // namespace MineClone

#endif // MINECLONE_CLIENT_GFX_GRAPHICS_HPP_
