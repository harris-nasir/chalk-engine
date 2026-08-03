# SDL3 platform backend — design

## Context

`engine.platform` is the contract module: `WindowDescription` (constructor
input) and `Window` (the shared resource every other plugin reads —
`title`, `width`, `height`, `should_close`). `PlatformWin32Plugin`
(`engine.platform.win32`) was a stub: it inserted a `Window` resource and
printed lifecycle messages, but never called a real Win32 API. SDL3 is
this project's first real platform backend, and its first real external
dependency — nothing in the build currently links any native library.

## Goal

Add `engine.platform.sdl3` (`PlatformSDL3Plugin`) that actually opens an
SDL3 window and detects when it's closed. Delete the win32 stub — it
never did anything real, so it's dead weight now that a working backend
exists. Scope: open + close only. No resize, focus, or input events this
pass — `Window` has no fields for those yet, and adding them is a
separate follow-up.

## Dependency acquisition

[CPM.cmake](https://github.com/cpm-cmake/CPM.cmake), via
`CPMFindPackage`: tries `find_package(SDL3)` first (uses an
already-installed system SDL3 if present), falls back to fetching and
building SDL3 from source if not found. Single-file CMake script, no
external tool/toolchain requirement beyond CMake itself.

```cmake
CPMFindPackage(
  NAME SDL3
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-3.x  # pin to latest stable 3.x tag at implementation time
)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
```

## Architecture

`engine.platform` (contract) is untouched. `PlatformSDL3Plugin` is the
only thing that ever touches an `SDL_Window*` or any other SDL3 type —
enforced by keeping the type holding it **non-exported** (declared inside
the module's implementation `namespace engine { ... }` block, not
`export namespace engine { ... }`), so it cannot be named from outside
this module even in principle, not merely by convention.

```cpp
// implementation-only, not exported
struct SdlWindowHandle
{
  SDL_Window* window;
};
```

### `PlatformSDL3Plugin`

Same constructor shape as the old win32 stub: `explicit
PlatformSDL3Plugin(WindowDescription description = {})`.

**`build(App& app)`** — synchronous, no `Startup` system needed:

1. `SDL_Init(SDL_INIT_VIDEO)`. On failure: fetch `Diagnostics`, `report()`
   the SDL error (`SDL_GetError()`), `drain_unprinted(print_diagnostic)`
   to flush it immediately, then `assert(false, ...)` — the same
   hard-fail idiom `App::resolve_parameter` already uses for a missing
   required resource. A window that silently fails to open is a worse
   failure mode than a loud, immediate crash with the SDL error attached.
2. `SDL_CreateWindow(title, width, height, 0)`. Same failure handling as
   step 1 if it returns null.
3. `insert_resource<Window>({.title = ..., .width = ..., .height = ...,
   .should_close = false})` — the contract resource.
4. `insert_resource<SdlWindowHandle>({.window = sdl_window})` — the
   private resource.
5. Report "opened window ..." directly via the already-fetched
   `Diagnostics` (`build()` has `App&` already; no need for `Commands`,
   which exists for *systems*, not plugin registration).

**`PreUpdate` system** — `(SdlWindowHandle&, Window&)`: pumps
`SDL_PollEvent` in a loop; on `SDL_EVENT_QUIT` or
`SDL_EVENT_WINDOW_CLOSE_REQUESTED`, sets `Window::should_close = true`.
Same schedule the win32 stub's should_close check used.

**`Shutdown` system** — `(SdlWindowHandle&, Commands)`: `SDL_DestroyWindow`
+ `SDL_Quit`, then `cmd.report("window closed")`.

Two systems total (`PreUpdate`, `Shutdown`) — not three. Window creation
isn't time-sensitive the way `CorePlugin`'s `Clock::now()` seed is (no
elapsed-time drift concern from happening at plugin-registration time
instead of loop-start), so there's no reason to defer it into a
`Startup` system.

## Error handling summary

Any SDL3 call in `build()` that can fail (`SDL_Init`, `SDL_CreateWindow`)
hard-fails: report the SDL error through `Diagnostics`, flush
synchronously, `assert(false, ...)`. Matches the engine's existing
philosophy (a missing required resource already hard-asserts rather than
degrading silently) and is consistent given exceptions are disabled
project-wide (`-fno-exceptions`/`/EHs-c-`).

## Cleanup

- Delete `engine/plugin/platform/win32/platform.win32.ixx`.
- `source/main.cxx`: `PlatformWin32Plugin` → `PlatformSDL3Plugin`.

## Out of scope (this pass)

- Window resize, focus/minimize, and input events — `Window` has no
  fields for these; adding them is a separate follow-up once needed.
- Multiple windows.
- Any renderer/graphics API tie-in (`engine.renderer.dx11` is unrelated
  to this change; SDL3 here is windowing only).
