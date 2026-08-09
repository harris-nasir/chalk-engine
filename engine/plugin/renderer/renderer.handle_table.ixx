module;

#include <optional>
#include <utility>
#include <vector>

export module engine.renderer.handle_table;

import engine.core;

export namespace engine
{

  // Generic 1-based index table shared by every renderer backend's resource
  // records (buffers/textures/shaders/pipelines): index 0 is reserved for
  // Invalid, and indices are never reused, so a stale handle reliably misses
  // instead of silently aliasing a newer resource.
  template <typename T>
  class HandleTable
  {
  public:
    auto insert(T value) -> u64
    {
      slots_.push_back(std::move(value));
      return slots_.size(); // 1-based; 0 is reserved for Invalid, and indices are never reused
    }

    [[nodiscard]] auto get(u64 handle) -> T*
    {
      if (handle == 0 || handle > slots_.size())
      {
        return nullptr;
      }
      auto& slot = slots_[handle - 1];
      return slot ? &*slot : nullptr;
    }

    auto destroy(u64 handle) -> bool
    {
      if (get(handle) == nullptr)
      {
        return false;
      }
      slots_[handle - 1].reset();
      return true;
    }

  private:
    std::vector<std::optional<T>> slots_;
  };

} // namespace engine
