export module engine.audio.xaudio2;

import engine.core;
import engine.audio;

export namespace engine
{

  class AudioXAudio2Plugin
  {
  public:
    void build(App& app);
  };

} // namespace engine

namespace engine
{

  void AudioXAudio2Plugin::build(App& app)
  {
    app.add_system(Schedule::Startup, [](App& app) { app.report(Severity::Info, "device opened"); });

    app.add_system(Schedule::PostUpdate, [](App&) {});

    app.add_system(Schedule::Shutdown, [](App& app) { app.report(Severity::Info, "device closed"); });
  }

} // namespace engine
