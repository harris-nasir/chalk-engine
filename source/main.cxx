import engine;

auto main() -> int
{
  using namespace engine;

  App app;
  app.add_plugin<CorePlugin>();

  app.add_system(
      Schedule::Update,
      [](Time& time, Diagnostics& diagnostics, ExitControl exit) -> void
      {
        diagnostics.report("Frame {}: {}", time.frame_count, time.delta_seconds);
        if (time.elapsed_seconds > 1)
        {
          exit.request();
        }
      }
  );

  app.execute();
}
