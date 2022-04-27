#pragma once
#ifndef MINECLONE_CLIENT_GFX_GAME_HPP_
#define MINECLONE_CLIENT_GFX_GAME_HPP_

#include "Graphics.hpp"

namespace MineClone
{

class Game
{
  public:
    Game(std::string title, size_t width, size_t height);

    virtual ~Game();

    NON_COPYABLE(Game);
    NON_MOVABLE(Game);

  public:
    void GameLoop();

    void Destroy();

    [[nodiscard]] size_t GetWidth() const noexcept;
    [[nodiscard]] size_t GetHeight() const noexcept;

  private:
    void Initialize();

  private:
    size_t m_width, m_height;
    const std::string m_title;

    bool m_hasGlfw{false};
    GLFWwindow *m_glWindow{nullptr};
    VulkanContext m_vulkanContext{};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
}; // class Window

} // namespace MineClone

#endif // MINECLONE_CLIENT_GFX_GAME_HPP_
