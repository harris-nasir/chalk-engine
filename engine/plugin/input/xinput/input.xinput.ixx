export module engine.input.xinput;

import engine.core;
import engine.input;

export namespace engine
{

  class InputXInputPlugin
  {
  public:
    void build(App& app);
  };

} // namespace engine

namespace engine
{

  void InputXInputPlugin::build(App& app)
  {
    app.insert_resource<InputState>({});

    app.add_system(Schedule::Startup, [](App& app) { app.report(Severity::Info, "xinput ready"); });

    app.add_system(Schedule::PreUpdate, [](App&) {});
  }

} // namespace engine
