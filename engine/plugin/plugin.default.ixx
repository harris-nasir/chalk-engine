module;

#include <utility>

export module engine.plugin_default;

import engine.core;
import engine.platform;

#if defined(CHALK_PLATFORM_SDL3)
import engine.platform.sdl3;
#elif defined(CHALK_PLATFORM_WIN32)
import engine.platform.win32;
#else
#error "unknown platform backend; define CHALK_PLATFORM_SDL3 or CHALK_PLATFORM_WIN32"
#endif

import engine.renderer;

#if defined(CHALK_RENDERER_SDL3)
import engine.renderer.sdl3;
#elif defined(CHALK_RENDERER_DX11)
import engine.renderer.dx11;
#else
#error "unknown renderer backend; define CHALK_RENDERER_SDL3 or CHALK_RENDERER_DX11"
#endif

#if defined(CHALK_AUDIO_XAUDIO2)
import engine.audio.xaudio2;
#endif

#if defined(CHALK_INPUT_XINPUT)
import engine.input.xinput;
#endif

#if defined(CHALK_PHYSICS_SIMPLE)
import engine.physics.simple;
#endif

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
#if defined(CHALK_PLATFORM_SDL3)
    app.add_plugin<PlatformSDL3Plugin>(description_);
#elif defined(CHALK_PLATFORM_WIN32)
    app.add_plugin<PlatformWin32Plugin>(description_);
#endif

#if defined(CHALK_RENDERER_SDL3)
    app.add_plugin<RendererSDL3Plugin>();
#elif defined(CHALK_RENDERER_DX11)
    app.add_plugin<RendererDX11Plugin>();
#endif

#if defined(CHALK_AUDIO_XAUDIO2)
    app.add_plugin<AudioXAudio2Plugin>();
#endif

#if defined(CHALK_INPUT_XINPUT)
    app.add_plugin<InputXInputPlugin>();
#endif

#if defined(CHALK_PHYSICS_SIMPLE)
    app.add_plugin<PhysicsSimplePlugin>();
#endif
  }

} // namespace engine
