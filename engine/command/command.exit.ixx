export module engine.command:exit;

import engine.core;

export namespace engine
{

  class ExitControl
  {
  public:
    explicit ExitControl(App& app) : app_(app) {}

    void request() { app_.request_exit(); }

  private:
    App& app_;
  };

} // namespace engine
