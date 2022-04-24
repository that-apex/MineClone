#include <MineClone/Client.hpp>

#include <MineClone/Game/MineCloneGame.hpp>

#include <iostream>

namespace MineClone
{

int Main(int argc, char **argv)
{
    try
    {
        MineCloneGame game;
        game.GameLoop();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        auto exception = dynamic_cast<const Exception *>(&e);

        return exception ? exception->ErrorCode() : -1;
    }

    return 0;
}

} // namespace MineClone
