module;

#include <utility>

export module engine.plugin_default;

import engine.core;
import engine.platform;
import engine.platform.sdl3;
import engine.platform.win32;

export namespace engine
{

  class DefaultPlugin
  {
  public:
    explicit DefaultPlugin(WindowDescription description = {}) : description_(std::move(description)) {}

    void build(App& app);

  private:
    WindowDescription description_;
  };

} // namespace engine

namespace engine
{

  void DefaultPlugin::build(App& app)
  {
    app.add_plugin<PlatformWin32Plugin>(description_);
    // app.add_plugin<PlatformSDL3Plugin>(description_);
  }

} // namespace engine
