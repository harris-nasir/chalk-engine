module;

#include <algorithm>
#include <any>
#include <chrono>
#include <cstdlib>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

export module engine.core;

export import :option;
export import :type;

namespace engine
{
  using Clock = std::chrono::steady_clock;
} // namespace engine

export namespace engine::detail
{
  // Implicitly built from a format literal at report() call sites.
  struct ReportFormat
  {
    std::string_view fmt;
    std::source_location loc;

    template <typename T>
    constexpr ReportFormat(const T& format, std::source_location location = std::source_location::current())
        : fmt(format), loc(location)
    {
    }
  };
} // namespace engine::detail

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
    FixedUpdate,
    Update,
    PostUpdate,
    Render,
    Shutdown,
  };

  using System = std::function<void(App&)>;

  enum class Severity : u8
  {
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
  };

  struct Time
  {
    f64 delta_seconds   = 0.0; // real wall-clock time for this frame
    f64 elapsed_seconds = 0.0; // real time since startup
    u32 frame_count     = 0;   // frames executed

    f64 fixed_delta_seconds   = 0.0; // fixed step duration: 1 / 60 s
    f64 fixed_elapsed_seconds = 0.0; // simulation time accumulated in fixed steps
    u64 fixed_frame_count     = 0;   // fixed steps executed
    f64 render_alpha          = 0.0; // [0,1): interpolation fraction between fixed steps
  };

  class App
  {
  public:
    App();
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

    auto add_system(Schedule schedule, System system) -> App&
    {
      systems_[schedule].push_back(std::move(system));
      return *this;
    }

    template <typename T>
    auto insert_resource(std::type_identity_t<T> resource) -> App&
    {
      resources_[std::type_index(typeid(T))] = std::make_shared<T>(std::move(resource));
      return *this;
    }

    template <typename T>
    auto remove_resource() -> App&
    {
      resources_.erase(std::type_index(typeid(T)));
      return *this;
    }

    template <typename T>
    [[nodiscard]] auto has_resource() const -> bool
    {
      return resources_.contains(std::type_index(typeid(T)));
    }

    // Reference to a resource that may or may not exist.
    template <typename T>
    [[nodiscard]] auto resource(std::source_location location = std::source_location::current()) -> Option<T>
    {
      auto it = resources_.find(std::type_index(typeid(T)));
      if (it == resources_.end())
      {
        return Option<T>{location};
      }
      auto* holder = std::any_cast<std::shared_ptr<T>>(&it->second);
      if (holder == nullptr)
      {
        return Option<T>{location};
      }

      return Option<T>{**holder};
    }

    // Reference to a resource that must exist.
    template <typename T>
    [[nodiscard]] auto require_resource(std::source_location location = std::source_location::current()) -> T&
    {
      auto option = resource<T>(location);
      if (!option)
      {
        report_missing_required_resource(std::format("required resource `{}` missing", typeid(T).name()), location);
      }
      return *option;
    }

    void execute();

    [[nodiscard]] auto is_running() const -> bool { return is_running_; }

    void exit() { is_running_ = false; }

    template <typename... Args>
    void report(Severity severity, detail::ReportFormat fmt, const Args&... args)
    {
      report_text(severity, std::vformat(fmt.fmt, std::make_format_args(args...)), fmt.loc);
    }

  private:
    void run_schedule(Schedule schedule)
    {
      for (auto& system : systems_[schedule])
      {
        system(*this);
      }
    }

    void run_schedule_reversed(Schedule schedule)
    {
      auto& systems = systems_[schedule];
      for (auto it = systems.rbegin(); it != systems.rend(); ++it)
      {
        (*it)(*this);
      }
    }

    void report_missing_required_resource(std::string detail, std::source_location location);
    void report_text(Severity severity, std::string text, std::source_location location);

    bool is_running_ = false;
    Clock::time_point last_tick_;
    f64 accumulator_ = 0.0;
    std::unordered_map<std::type_index, std::any> resources_;
    std::unordered_map<Schedule, std::vector<System>> systems_;
  };

} // namespace engine

namespace engine
{
  namespace
  {
    struct DiagnosticMessage
    {
      Severity severity;
      std::string text;
      std::source_location location;
    };

    struct Diagnostics
    {
      Diagnostics()  = default;
      ~Diagnostics() = default;

      Diagnostics(const Diagnostics&)                        = delete;
      auto operator=(const Diagnostics&) -> Diagnostics&     = delete;
      Diagnostics(Diagnostics&&) noexcept                    = default;
      auto operator=(Diagnostics&&) noexcept -> Diagnostics& = default;

      template <typename... Args>
      void report(Severity severity, detail::ReportFormat fmt, const Args&... args)
      {
        push(severity, std::vformat(fmt.fmt, std::make_format_args(args...)), fmt.loc);
      }

      // Skips vformat, for text that's already formatted and may contain
      // literal braces (vformat would misread them as placeholders).
      void report_verbatim(Severity severity, std::string text, std::source_location location)
      {
        push(severity, std::move(text), location);
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
      // Fatal flushes and aborts immediately instead of waiting for the
      // next scheduled drain.
      void push(Severity severity, std::string text, std::source_location location);

      std::vector<DiagnosticMessage> messages_;
      std::size_t printed_ = 0;
    };

    [[nodiscard]] constexpr auto severity_name(Severity severity) -> std::string_view
    {
      switch (severity)
      {
        case Severity::Debug:
          return "DEBUG";
        case Severity::Info:
          return "INFO";
        case Severity::Warn:
          return "WARN";
        case Severity::Error:
          return "ERROR";
        case Severity::Fatal:
          return "FATAL";
      }
      return "UNKNOWN";
    }

    [[nodiscard]] constexpr auto severity_color(Severity severity) -> std::string_view
    {
      switch (severity)
      {
        case Severity::Debug:
          return "\033[90m"; // white
        case Severity::Info:
          return "\033[37m"; // grey
        case Severity::Warn:
          return "\033[33m"; // yellow
        case Severity::Error:
          return "\033[31m"; // red
        case Severity::Fatal:
          return "\033[1;31m"; // bold red
      }
      return "\033[0m";
    }

    inline constexpr std::string_view SEVERITY_COLOR_RESET = "\033[0m";

    inline void print_diagnostic(const DiagnosticMessage& entry)
    {
      std::string_view path = entry.location.file_name();
      auto file_start       = path.find_last_of("/\\");
      std::string_view file = file_start == std::string_view::npos ? path : path.substr(file_start + 1);
      auto tag_end          = file.find('.');
      std::string_view tag  = tag_end == std::string_view::npos ? file : file.substr(0, tag_end);
      std::cout << severity_color(entry.severity) << "[" << tag << "][" << severity_name(entry.severity) << "] " << file
                << ":" << entry.location.line() << ": " << entry.text << SEVERITY_COLOR_RESET << '\n';
    }

  } // namespace

  App::App() : last_tick_(Clock::now())
  {
    insert_resource<Diagnostics>({});
    insert_resource<Time>({});
  }

  void App::report_text(Severity severity, std::string text, std::source_location location)
  {
    require_resource<Diagnostics>().report_verbatim(severity, std::move(text), location);
  }

  void App::report_missing_required_resource(std::string detail, std::source_location location)
  {
    if (auto diagnostics = resource<Diagnostics>(location); diagnostics)
    {
      diagnostics->report_verbatim(Severity::Fatal, std::move(detail), location);
      return;
    }

    print_diagnostic(DiagnosticMessage{.severity = Severity::Fatal, .text = std::move(detail), .location = location});
    std::abort();
  }

  void Diagnostics::push(Severity severity, std::string text, std::source_location location)
  {
    messages_.push_back(DiagnosticMessage{.severity = severity, .text = std::move(text), .location = location});
    if (severity == Severity::Fatal)
    {
      drain_unprinted(print_diagnostic);
      std::abort();
    }
  }

  void App::execute()
  {
    constexpr f64 fixed_delta = 1.0 / 60.0;
    constexpr u32 max_steps   = 5;

    run_schedule(Schedule::Startup);
    is_running_ = true;

    auto& time        = require_resource<Time>();
    auto& diagnostics = require_resource<Diagnostics>();

    while (is_running_)
    {
      const auto now     = Clock::now();
      time.delta_seconds = std::chrono::duration<f64>(now - last_tick_).count();
      time.elapsed_seconds += time.delta_seconds;
      time.frame_count += 1;
      last_tick_ = now;

      time.fixed_delta_seconds = fixed_delta;

      accumulator_ += time.delta_seconds;
      const auto max_step_time = static_cast<f64>(max_steps) * fixed_delta;
      accumulator_             = std::min(accumulator_, max_step_time);

      run_schedule(Schedule::PreUpdate);

      u32 steps = 0;
      while (accumulator_ >= fixed_delta && steps < max_steps)
      {
        run_schedule(Schedule::FixedUpdate);
        time.fixed_elapsed_seconds += fixed_delta;
        time.fixed_frame_count += 1;
        accumulator_ -= fixed_delta;
        ++steps;
      }
      time.render_alpha = accumulator_ / fixed_delta;

      run_schedule(Schedule::Update);
      run_schedule(Schedule::PostUpdate);
      run_schedule(Schedule::Render);

      diagnostics.drain_unprinted(print_diagnostic);
    }

    run_schedule_reversed(Schedule::Shutdown);
    diagnostics.drain_unprinted(print_diagnostic);
  }

} // namespace engine
