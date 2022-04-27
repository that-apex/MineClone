#include <MineClone/GFX/Game.hpp>

namespace MineClone
{

Game::Game(std::string title, size_t width, size_t height) : m_title{std::move(title)}, m_width{width}, m_height{height}
{
}

Game::~Game()
{
    Destroy();
}

void Game::GameLoop()
{
    Initialize();

    while (!glfwWindowShouldClose(m_glWindow))
    {
        if (glfwGetKey(m_glWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(m_glWindow, true);
            break;
        }

        m_vulkanContext.Render();

        glfwPollEvents();
    }
}

void Game::Initialize()
{
    // glfw init
    if (!glfwInit())
        throw Exception("Failed to initialize GLFW");

    m_hasGlfw = true;

    // create window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_glWindow = glfwCreateWindow(static_cast<int>(m_width), static_cast<int>(m_height), m_title.c_str(), nullptr, nullptr);

    if (!m_glWindow)
        throw Exception("glfwCreateWindow failed");

    glfwSetWindowUserPointer(m_glWindow, static_cast<void *>(this));

    // init vulkan context
    m_vulkanContext.Initialize(m_glWindow);

}

void Game::Destroy()
{
    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_vulkanContext.GetInstance(), m_surface, nullptr);
        m_surface = nullptr;
    }

    m_vulkanContext.Destroy();

    if (m_glWindow)
    {
        glfwDestroyWindow(m_glWindow);
        m_glWindow = nullptr;
    }

    if (m_hasGlfw)
    {
        glfwTerminate();
        m_hasGlfw = false;
    }
}

size_t Game::GetWidth() const noexcept
{
    return m_width;
}

size_t Game::GetHeight() const noexcept
{
    return m_height;
}

} // namespace MineClone