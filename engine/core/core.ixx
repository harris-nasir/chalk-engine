module;

#include <any>
#include <cassert>
#include <chrono>
#include <format>
#include <functional>
#include <iostream>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

export module engine.core;

export import :option;
export import :type;

export namespace engine
{
  class App;

  template <typename T>
  concept Plugin = requires(T plugin, App& app) {
    { plugin.build(app) };
  };

  enum class Schedule : u8
  {
    Startup,
    PreUpdate,
    Update,
    PostUpdate,
    Render,
    Shutdown,
  };

  [[nodiscard]] constexpr auto schedule_name(Schedule schedule) -> std::string_view
  {
    switch (schedule)
    {
      case Schedule::Startup:
        return "Startup";
      case Schedule::PreUpdate:
        return "PreUpdate";
      case Schedule::Update:
        return "Update";
      case Schedule::PostUpdate:
        return "PostUpdate";
      case Schedule::Render:
        return "Render";
      case Schedule::Shutdown:
        return "Shutdown";
    }
    return "Unknown";
  }

  // Where a system was registered (app.add_system(...) call site), not
  // where it's currently executing — captured once at registration via a
  // default argument, so a missing-required-resource assert can point at
  // the plugin file/line that declared the system, not at core.ixx.
  struct SystemInfo
  {
    Schedule schedule;
    std::source_location location;
  };

  using System = std::function<void(App&)>;

  class App
  {
  public:
    App()  = default;
    ~App() = default;

    App(const App&)                    = delete;
    auto operator=(const App&) -> App& = delete;
    App(App&&)                         = delete;
    auto operator=(App&&) -> App&      = delete;

    template <Plugin Plugin, typename... Args>
    auto add_plugin(Args&&... args) -> App&
    {
      Plugin plugin{std::forward<Args>(args)...};
      plugin.build(*this);
      return *this;
    }

    template <typename T>
    auto insert_resource(std::type_identity_t<T> resource) -> App&
    {
      resources_[std::type_index(typeid(T))] = std::move(resource);
      return *this;
    }

    template <typename T>
    auto remove_resource() -> App&
    {
      resources_.erase(std::type_index(typeid(T)));
      return *this;
    }

    template <typename T>
    [[nodiscard]] auto resource(std::source_location location = std::source_location::current()) -> Option<T>
    {
      auto it = resources_.find(std::type_index(typeid(T)));
      if (it == resources_.end())
      {
        return Option<T>{location};
      }
      auto* value = std::any_cast<T>(&it->second);
      if (value == nullptr)
      {
        return Option<T>{location};
      }

      return Option<T>{*value};
    }

    template <typename T>
    [[nodiscard]] auto has_resource() const -> bool
    {
      return resources_.contains(std::type_index(typeid(T)));
    }

    template <typename F>
    auto add_system(Schedule schedule, F system, std::source_location location = std::source_location::current())
        -> App&;

    void execute()
    {
      run_schedule(Schedule::Startup);

      running_ = true;
      while (running_)
      {
        run_schedule(Schedule::PreUpdate);
        run_schedule(Schedule::Update);
        run_schedule(Schedule::PostUpdate);
        run_schedule(Schedule::Render);
      }

      run_schedule(Schedule::Shutdown);
    }

    void request_exit() { running_ = false; }

  private:
    template <typename F>
    struct function_traits : function_traits<decltype(&F::operator())>
    {
    };

    template <typename C, typename R, typename... Args>
    struct function_traits<R (C::*)(Args...)>
    {
      using args = std::tuple<Args...>;
    };

    template <typename C, typename R, typename... Args>
    struct function_traits<R (C::*)(Args...) const>
    {
      using args = std::tuple<Args...>;
    };

    template <typename Arg>
    auto resolve_parameter(SystemInfo info) -> Arg;

    template <typename F, typename... Args>
    auto make_system_impl(SystemInfo info, F system, std::tuple<Args...>* /*unused*/) -> System;

    template <typename F>
    auto make_system(SystemInfo info, F system) -> System;

    void run_schedule(Schedule schedule)
    {
      for (auto& system : systems_[schedule])
      {
        system(*this);
      }
    }

    bool running_ = false;
    std::unordered_map<std::type_index, std::any> resources_;
    std::unordered_map<Schedule, std::vector<System>> systems_;
  };

  struct Time
  {
    f64 delta_seconds   = 0.0;
    f64 elapsed_seconds = 0.0;
    u32 frame_count     = 0;
  };

  struct DiagnosticMessage
  {
    std::string text;
    std::source_location location;
  };

  struct ReportFormat
  {
    std::string_view fmt;
    std::source_location location;

    template <typename T>
    constexpr ReportFormat(const T& format, std::source_location loc = std::source_location::current())
        : fmt(format), location(loc)
    {
    }
  };

  struct Diagnostics
  {
    template <typename... Args>
    void report(ReportFormat fmt, Args&&... args)
    {
      messages_.push_back(
          DiagnosticMessage{.text = std::vformat(fmt.fmt, std::make_format_args(args...)), .location = fmt.location}
      );
    }

    // For a message already fully formatted elsewhere (e.g.
    // App::resolve_parameter's missing-resource diagnostic) — skips
    // report()'s vformat, since re-running an already-resolved string
    // through it would try to parse any literal '{'/'}' the string
    // contains as placeholders, and — exceptions are disabled in this
    // project — an unbalanced brace would call std::terminate() instead
    // of throwing.
    void report_verbatim(std::string text, std::source_location location)
    {
      messages_.push_back(DiagnosticMessage{.text = std::move(text), .location = location});
    }

    template <typename Fn>
    void drain_unprinted(Fn&& function)
    {
      while (printed_ < messages_.size())
      {
        std::forward<Fn>(function)(messages_[printed_]);
        ++printed_;
      }
    }

  private:
    std::vector<DiagnosticMessage> messages_;
    std::size_t printed_ = 0;
  };

  inline void print_diagnostic(const DiagnosticMessage& entry)
  {
    std::string_view path = entry.location.file_name();
    auto file_start        = path.find_last_of("/\\");
    std::string_view file  = file_start == std::string_view::npos ? path : path.substr(file_start + 1);
    auto tag_end           = file.find('.');
    std::string_view tag   = tag_end == std::string_view::npos ? file : file.substr(0, tag_end);
    std::cout << "[" << tag << "] " << file << ":" << entry.location.line() << ": " << entry.text << std::endl;
  }

  class CorePlugin
  {
  public:
    static void build(App& app);
  };

} // namespace engine

namespace engine
{

  template <typename Arg>
  auto App::resolve_parameter(SystemInfo info) -> Arg
  {
    using Bare = std::remove_cvref_t<Arg>;
    if constexpr (std::is_same_v<Bare, App>)
    {
      return *this;
    }
    else if constexpr (std::is_constructible_v<Bare, App&>)
    {
      return Bare{*this};
    }
    else if constexpr (IS_OPTION_V<Bare>)
    {
      return resource<typename is_option<Bare>::value_type>(info.location);
    }
    else
    {
      auto option = resource<Bare>(info.location);
      if (!option)
      {
        if (auto diagnostics = resource<Diagnostics>(); diagnostics)
        {
          diagnostics->report_verbatim(
              std::format(
                  "system in Schedule::{} requires missing resource `{}`", schedule_name(info.schedule),
                  typeid(Bare).name()
              ),
              info.location
          );
          diagnostics->drain_unprinted(print_diagnostic);
        }
      }
      assert(option && "required resource missing");
      return *option;
    }
  }

  template <typename F, typename... Args>
  auto App::make_system_impl(SystemInfo info, F system, std::tuple<Args...>* /*unused*/) -> System
  {
    return [info, system = std::move(system)](App& app) mutable -> void
    {
      (void)info; // unused when the system takes no resource parameters
      system(app.resolve_parameter<Args>(info)...);
    };
  }

  template <typename F>
  auto App::make_system(SystemInfo info, F system) -> System
  {
    using Args = typename function_traits<F>::args;
    return make_system_impl(info, std::move(system), static_cast<Args*>(nullptr));
  }

  template <typename F>
  auto App::add_system(Schedule schedule, F system, std::source_location location) -> App&
  {
    systems_[schedule].push_back(
        make_system(SystemInfo{.schedule = schedule, .location = location}, std::move(system))
    );
    return *this;
  }

  namespace
  {
    using Clock = std::chrono::steady_clock;
  } // namespace

  void CorePlugin::build(App& app)
  {
    app.insert_resource<Time>({});
    app.insert_resource<Diagnostics>({});

    app.add_system(Schedule::Startup, [](App& app) -> void { app.insert_resource<Clock::time_point>(Clock::now()); });

    app.add_system(
        Schedule::PreUpdate,
        [](Clock::time_point& last_tick, Time& time) -> void
        {
          const auto now     = Clock::now();
          time.delta_seconds = std::chrono::duration<f64>(now - last_tick).count();
          time.elapsed_seconds += time.delta_seconds;
          time.frame_count += 1;
          last_tick = now;
        }
    );

    app.add_system(
        Schedule::PreUpdate, [](Diagnostics& diagnostics) -> void { diagnostics.drain_unprinted(print_diagnostic); }
    );
  }

} // namespace engine
