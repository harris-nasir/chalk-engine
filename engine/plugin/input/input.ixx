module;
#include <array>

export module engine.input;
import engine.core;

export namespace engine
{

  struct InputState
  {
    std::array<bool, 256> key_down;
  };

} // namespace engine
