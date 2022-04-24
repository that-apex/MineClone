#pragma once
#ifndef MINECLONE_CLIENT_GAME_MINECLONEGAME_HPP_
#define MINECLONE_CLIENT_GAME_MINECLONEGAME_HPP_

#include "../Common.hpp"

#include "../GFX/Game.hpp"

namespace MineClone
{

class MineCloneGame : public Game {
  public:
    MineCloneGame();

    void Render() override;
};

} // namespace MineClone

#endif // MINECLONE_CLIENT_GAME_MINECLONEGAME_HPP_
