module;

#include <cassert>
#include <functional>
#include <iostream>
#include <optional>
#include <source_location>
#include <string_view>
#include <type_traits>

export module engine.core:option;

export namespace engine
{
  // Non-owning, possibly-empty reference to a resource. Carries the
  // location it was obtained from, printed if dereferenced while empty.
  template <typename T>
  class Option
  {
  public:
    constexpr explicit Option(std::source_location location = std::source_location::current()) : location_(location) {}

    constexpr explicit Option(T& value, std::source_location location = std::source_location::current())
        : value_(value), location_(location)
    {
    }

    constexpr Option(
        std::optional<std::reference_wrapper<T>> value, std::source_location location = std::source_location::current()
    )
        : value_(value), location_(location)
    {
    }

    [[nodiscard]] constexpr auto operator->() const -> T*
    {
      check();
      return &value_->get();
    }

    [[nodiscard]] constexpr auto operator*() const -> T&
    {
      check();
      return value_->get();
    }

    [[nodiscard]] constexpr explicit operator bool() const { return value_.has_value(); }

  private:
    constexpr void check() const
    {
      if (!value_)
      {
        std::string_view path = location_.file_name();
        auto pos               = path.find_last_of("/\\");
        std::string_view file  = pos == std::string_view::npos ? path : path.substr(pos + 1);
        std::cerr << "[engine] " << location_.function_name() << ": " << file << ":" << location_.line()
                  << ": dereferenced an empty Option\n";
      }
      assert(value_ && "dereferenced an empty Option");
    }

    std::optional<std::reference_wrapper<T>> value_;
    std::source_location location_;
  };

  template <typename T>
  struct is_option : std::false_type
  {
  };

  template <typename T>
  struct is_option<Option<T>> : std::true_type
  {
    using value_type = T;
  };

  template <typename T>
  inline constexpr bool IS_OPTION_V = is_option<T>::value;

} // namespace engine
