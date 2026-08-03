export module engine.physics;

import engine.core;

export namespace engine
{

  struct PhysicsWorld
  {
    f32 gravity    = -9.81F;
    u32 step_count = 0;
  };

} // namespace engine
