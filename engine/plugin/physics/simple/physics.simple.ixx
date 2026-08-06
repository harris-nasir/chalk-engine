export module engine.physics.simple;

import engine.core;
import engine.physics;

export namespace engine
{

  class PhysicsSimplePlugin
  {
  public:
    void build(App& app);
  };

} // namespace engine

namespace engine
{

  void PhysicsSimplePlugin::build(App& app)
  {
    app.insert_resource<PhysicsWorld>({});

    app.add_system(
        Schedule::Update,
        [](App& app) -> void
        {
          auto& world = app.require_resource<PhysicsWorld>();
          auto& time  = app.require_resource<Time>();
          world.step_count += 1;
          app.report(
              Severity::Info, "step {:>3} (g={:.2f}, dt {:.4f}s)", world.step_count, world.gravity, time.delta_seconds
          );
        }
    );
  }

} // namespace engine
