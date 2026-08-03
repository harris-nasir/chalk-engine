import engine;
using namespace engine;

auto main() -> int
{
  App app;
  app.add_plugin<DefaultPlugin>(WindowDescription{.title = "Engine"}).execute();
}
