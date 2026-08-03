module;

#include <type_traits>
#include <utility>

export module engine.command;

export import :exit;

import engine.core;

export namespace engine
{

  // Narrow, App-backed capability for systems: insert/remove resources,
  // report diagnostics, without full App& access.
  class Commands
  {
  public:
    explicit Commands(App& app) : app_(app) {}

    // T must be named explicitly (non-deduced context).
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

    // e.g. cmd.report(Severity::Info, "frame {}", frame_count).
    template <typename... Args>
    void report(Severity severity, ReportFormat fmt, Args&&... args)
    {
      diagnostics_or_abort(app_).report(severity, fmt, std::forward<Args>(args)...);
    }

  private:
    App& app_;
  };

} // namespace engine
