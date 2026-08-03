module;

#include <utility>

export module engine.plugin.standard;

import engine.core;
import engine.platform;
import engine.platform.sdl3;

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
    app.add_plugin<CorePlugin>();
    app.add_plugin<PlatformSDL3Plugin>(description_);
  }

} // namespace engine
