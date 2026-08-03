module;

#include <cassert>
#include <type_traits>
#include <utility>

export module engine.command;

export import :exit;

import engine.core;

export namespace engine
{

  // Narrow, App-backed capability: insert or remove resources, or report a
  // diagnostic message. A system that only needs to do these things takes
  // Commands instead of App&, so its signature can't also add plugins, add
  // systems, or call execute()/request_exit().
  class Commands
  {
  public:
    explicit Commands(App& app) : app_(app) {}

    // std::type_identity_t<T>: non-deduced context, so T must be named
    // explicitly at the call site (matches App::insert_resource).
    template <typename T>
    void insert_resource(std::type_identity_t<T> resource)
    {
      app_.insert_resource<T>(std::move(resource));
    }

    template <typename T>
    void remove_resource()
    {
      app_.remove_resource<T>();
    }

    // ReportFormat (engine.core) carries the format string + its own
    // call-site-captured source_location; args are the std::format
    // arguments, e.g. cmd.report("frame {}", frame_count).
    template <typename... Args>
    void report(ReportFormat fmt, Args&&... args)
    {
      auto diagnostics = app_.resource<Diagnostics>();
      assert(diagnostics && "Diagnostics resource missing. `CorePlugin` must be added before any other plugin");
      diagnostics->report(fmt, std::forward<Args>(args)...);
    }

  private:
    App& app_;
  };

} // namespace engine
