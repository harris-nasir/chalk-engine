module;

#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

export module engine.scene;

import engine.core;
import engine.math;
import engine.renderer.types;

export namespace engine
{

  enum class EntityID : u64
  {
    Invalid = 0
  };

  struct Transform
  {
    Vector3 position{};
    f32 rotation = 0.0F; // radians, around z
    Vector2 scale{.x = 1.0F, .y = 1.0F};
  };

  struct Renderable
  {
    PipelineHandle pipeline = PipelineHandle::Invalid;
    BufferHandle vbo, ibo;
    u32 index_count       = 0;
    TextureHandle texture = TextureHandle::Invalid;
  };

} // namespace engine

namespace engine
{

  [[nodiscard]] constexpr auto make_entity_id(u32 index, u32 generation) -> EntityID
  {
    return static_cast<EntityID>((static_cast<u64>(generation) << 32) | static_cast<u64>(index));
  }

  [[nodiscard]] constexpr auto entity_index(EntityID id) -> u32
  {
    return static_cast<u32>(static_cast<u64>(id) & 0xFFFFFFFFU);
  }

  [[nodiscard]] constexpr auto entity_generation(EntityID id) -> u32
  {
    return static_cast<u32>(static_cast<u64>(id) >> 32);
  }

  // Generational index allocator: recycled slots get a bumped generation so
  // a stale EntityID reads as dead instead of aliasing a new entity.
  class EntityAllocator
  {
  public:
    [[nodiscard]] auto allocate() -> EntityID
    {
      if (!free_indices_.empty())
      {
        u32 index = free_indices_.back();
        free_indices_.pop_back();
        return make_entity_id(index, generations_[index - 1]);
      }

      generations_.push_back(0);
      auto index = static_cast<u32>(generations_.size()); // 1-based, 0 reserved for Invalid
      return make_entity_id(index, 0);
    }

    auto free(EntityID id) -> void
    {
      if (!is_alive(id))
      {
        return;
      }

      u32 index = entity_index(id);
      ++generations_[index - 1];
      free_indices_.push_back(index);
    }

    [[nodiscard]] auto is_alive(EntityID id) const -> bool
    {
      u32 index = entity_index(id);
      if (id == EntityID::Invalid || index == 0 || index > generations_.size())
      {
        return false;
      }
      return generations_[index - 1] == entity_generation(id);
    }

  private:
    std::vector<u32> generations_;
    std::vector<u32> free_indices_;
  };

} // namespace engine

export namespace engine
{

  class World
  {
  public:
    [[nodiscard]] auto spawn() -> EntityID { return allocator_.allocate(); }

    auto destroy(EntityID id) -> void
    {
      if (!allocator_.is_alive(id))
      {
        return;
      }
      allocator_.free(id);
      transforms_.erase(id);
      renderables_.erase(id);
    }

    [[nodiscard]] auto is_alive(EntityID id) const -> bool { return allocator_.is_alive(id); }

    template <typename T>
    auto add(EntityID id, T value) -> void
    {
      storage<T>().insert_or_assign(id, std::move(value));
    }

    template <typename T>
    auto remove(EntityID id) -> void
    {
      storage<T>().erase(id);
    }

    template <typename T>
    [[nodiscard]] auto has(EntityID id) const -> bool
    {
      return storage<T>().contains(id);
    }

    template <typename T>
    [[nodiscard]] auto get(EntityID id) -> Option<T>
    {
      auto& map = storage<T>();
      auto it   = map.find(id);
      if (it == map.end())
      {
        return Option<T>{};
      }
      return Option<T>{it->second};
    }

    // Iterates the first listed component's map, yielding each entity that
    // has every listed component. List rarer components first.
    template <typename... Ts>
    [[nodiscard]] auto view() -> std::vector<std::tuple<EntityID, Ts&...>>
    {
      std::vector<std::tuple<EntityID, Ts&...>> result;
      using Primary = std::tuple_element_t<0, std::tuple<Ts...>>;

      for (auto& entry : storage<Primary>())
      {
        EntityID id = entry.first;
        if ((has<Ts>(id) && ...))
        {
          result.emplace_back(id, *get<Ts>(id)...);
        }
      }
      return result;
    }

  private:
    template <typename T>
    [[nodiscard]] auto storage() -> std::unordered_map<EntityID, T>&
    {
      if constexpr (std::is_same_v<T, Transform>)
      {
        return transforms_;
      }
      else if constexpr (std::is_same_v<T, Renderable>)
      {
        return renderables_;
      }
      else
      {
        static_assert(sizeof(T) == 0, "World: unknown component type");
      }
    }

    template <typename T>
    [[nodiscard]] auto storage() const -> const std::unordered_map<EntityID, T>&
    {
      if constexpr (std::is_same_v<T, Transform>)
      {
        return transforms_;
      }
      else if constexpr (std::is_same_v<T, Renderable>)
      {
        return renderables_;
      }
      else
      {
        static_assert(sizeof(T) == 0, "World: unknown component type");
      }
    }

    EntityAllocator allocator_;
    std::unordered_map<EntityID, Transform> transforms_;
    std::unordered_map<EntityID, Renderable> renderables_;
  };

  class ScenePlugin
  {
  public:
    void build(App& app);
  };

} // namespace engine

namespace engine
{

  void ScenePlugin::build(App& app) { app.insert_resource<World>({}); }

} // namespace engine
