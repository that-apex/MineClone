#include <MineClone/GFX/Graphics.hpp>

namespace MineClone
{
GraphicsException::GraphicsException(std::string message) : Exception(std::move(message))
{
}
} // namespace MineClone
