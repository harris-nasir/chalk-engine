# Renderer contract: game-first extraction design

Date: 2026-08-06
Status: implemented — SDL3 backend + first feature modules shipped; DX11 migration pending (step 6)

## Goal

Define the `engine.renderer` contract (core) plus the feature-extension pattern so that:

- anything can be built on top of it without restrictions,
- adding features never causes regressions or conflicts with existing contract pieces,
- every backend (SDL3 GPU today, DX11, future software renderer) produces identical pixel output.

Validation vehicle: a **Pong** game written first against raw SDL3, from which the API surface is extracted.

## Decisions (locked)

### 1. Recording model: open command list

- `FrameCommands` (core resource, inserted by the base renderer plugin) holds type-erased entries: `std::vector<std::any>`.
- **Game code never pushes commands.** Game code sets *data* (resources). Feature plugins convert data to commands in `Render` systems. The backend is the only reader of `FrameCommands`.
- Commands are plain data, enforced by concept:

```cpp
template <typename T>
concept RenderCommand = std::is_trivially_copyable_v<T>; // memcpy-able, byte-comparable, arena-storable later

struct FrameCommands
{
  std::vector<std::any> entries;
  template <RenderCommand T> void push(T command) { entries.emplace_back(command); }
};
```

### 2. Extension model: tiny stable core + per-feature modules

- `engine.renderer` (core) owns only frame lifecycle: `Color`, `FrameCommands`, `ClearColor`, the `RenderCommand` concept, and a fold-based `dispatch` helper. It never grows with feature concerns.
- Each feature is its own module (e.g. `engine.renderer.sprite`) defining its own command structs, resources, and pixel-semantics spec.
- Backends declare the commands they implement and translate via a fold dispatch (first match wins, `if constexpr` per command):

```cpp
template <RenderCommand... Supported, typename Visitor>
auto dispatch(const std::any& entry, Visitor&& visit) -> bool
{
  return (try_dispatch<Supported>(entry, visit) || ...);
}

// backend usage:
if (!dispatch<DrawQuad>(entry, [&](const auto& command) {
      using T = std::remove_cvref_t<decltype(command)>;
      if constexpr (std::same_as<T, DrawQuad>) { /* translate */ }
    }))
  app.report(Severity::Error, "unsupported command"); // once per type per frame
```

- **Unknown command** (backend doesn't implement it): report `Severity::Error`, once per command type per frame, skip, never crash. Parity tests catch drift at CI time, not runtime.

### 3. Frame lifecycle: base owns clear -> execute -> present

- `Schedule` gains `PostRender` (Render -> PostRender -> loop): a one-line core change so the backend's execute runs after feature plugins' pushes.
- Base plugin (per backend) `PostRender` system, every frame:
  1. clear from the `ClearColor` resource (default black, inserted if missing)
  2. execute `FrameCommands` entries in push order (draw order = push order)
  3. present
  4. drain the list (each frame starts empty, by construction)
- Clear is **implicit frame lifecycle, not a command**: `ClearColor` resource, set by game code, read by the base. (This is why "inserting ClearColor" registers no command — the base's already-registered `PostRender` system reads it every frame.)

### 4. Game data model: Design B now, ECS later

- Game structs own their visuals (`struct Paddle { f32 x, y, width, height; ... }`); a store holds registered borrows (pointers) so the plugin can find them. Rule: stable addresses (`unique_ptr`/`deque`).
- The renderer contract is ECS-agnostic; flecs or a hand-rolled ECS can replace the data model later without touching `FrameCommands` or the backends.

### 5. Parity

- Every command's pixel semantics are specified in its owning contract module.
- Backends must produce identical output; the first parity check is Pong raw vs Pong on the contract. A future software renderer consumes the same command list on a CPU path.

## Plan (game-first extraction)

1. **Write Pong on the engine** (App + schedules + systems), all SDL3 renderer calls confined to one module so they are greppable/extractable. Scope: clear, two paddles, ball, center line, optional score text (`SDL_RenderDebugText`), present. Input via the existing SDL3 platform plugin's event pump (keyboard). No textures, no camera.
2. **Categorize every SDL touch point**:

   | SDL3 call | category | future home |
   |---|---|---|
   | `SDL_CreateRenderer` / `SDL_DestroyRenderer` | lifecycle | backend plugin Startup/Shutdown |
   | `SDL_SetRenderDrawColor` + `SDL_RenderClear` | frame state | `ClearColor` resource (base PostRender) |
   | `SDL_RenderFillRect` | draw content | `DrawQuad` command (feature) |
   | `SDL_RenderDebugText` | draw content | text feature (or defer score) |
   | `SDL_RenderPresent` | frame lifecycle | base PostRender |
   | `SDL_GetRendererOutputSize` | query | `Window` resource (already exists) |

3. **Extract the contract**: `engine.renderer` core as above; **decided from Pong's evidence**: `DrawQuad` lives in feature module `engine.renderer.sprite`, `DrawText` (score) in `engine.renderer.text` — both shipped as trivially-copyable commands. Feature modules are exported by the `engine` facade alongside the core contract.
4. **Rewrite Pong against the contract** — output must match the raw version (parity check #1).
5. **Turn the raw module into the SDL3 backend** implementing the contract: `engine.renderer.sdl3` reworked — base resources (`FrameCommands`, `ClearColor`) + `PostRender` clear → dispatch → present → drain; unknown command → Error once per type per frame.
6. **Follow-ups** (out of scope here): DX11 backend + cross-backend parity test; texture handles (draw a PNG); camera feature; flecs migration; input contract extraction (keyboard state currently read via `SDL_GetKeyboardState` in game code); supersede ADR-0001 with ADR-0002 when the DX11 backend also lands.

## Target game-code shape (post-extraction)

```cpp
// game code owns Sprite/Text data and mutates it directly (no renderer in sight)
struct Paddle { engine::Sprite sprite; f32 speed; };

// build(): register visuals with the engine's feature plugins once
app.insert_resource<ClearColor>({.color = Color{0.1f, 0.1f, 0.1f, 1.f}});
sprites.register_sprite(game.left.sprite);
texts.register_text(game.score);

// update systems mutate sprite.x / sprite.color / text buffer directly
// SpritePlugin/TextPlugin (Render) convert that data into DrawQuad/DrawText pushes
// backend PostRender: clear -> execute -> present -> drain
```

## Deferred (YAGNI)

- Texture handles (`SDL_Texture` vs `ID3D11Texture2D` vs future CPU pixel buffer) — the open design makes `DrawTexture` a conflict-free later add.
- Camera / transforms.
- General system-ordering API (`PostRender` covers the one real ordering need).
- Per-frame arena for command storage (`std::any` is fine at 2D scale).
