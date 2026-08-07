# SDL3 platform backend: design

## Context

`engine.platform` is the contract module: `WindowDescription` (constructor
input) and `Window` (the shared resource every other plugin reads:
`title`, `width`, `height`, `should_close`). `PlatformWin32Plugin`
(`engine.platform.win32`) was a stub: it inserted a `Window` resource and
printed lifecycle messages, but never called a real Win32 API. SDL3 is
this project's first real platform backend, and its first real external
dependency: nothing in the build currently links any native library.

## Goal

Add `engine.platform.sdl3` (`PlatformSDL3Plugin`) that actually opens an
SDL3 window and detects when it's closed. Delete the win32 stub: it
never did anything real, so it's dead weight now that a working backend
exists. Scope: open + close only. No resize, focus, or input events this
pass; `Window` has no fields for those yet, and adding them is a
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

`engine.platform` (contract) gains one addition beyond `Window` itself,
see "Native window handle" below. `PlatformSDL3Plugin` is the only thing
that ever touches an `SDL_Window*` or any other SDL3 type, enforced by
keeping the type holding it **non-exported** (declared inside the
module's implementation `namespace engine { ... }` block, not
`export namespace engine { ... }`), so it cannot be named from outside
this module even in principle, not merely by convention.

```cpp
// implementation-only, not exported
struct SdlWindowHandle
{
  SDL_Window* window;
};
```

### Native window handle

`Window` stays platform-agnostic (title/width/height/should_close:
properties every windowing system has). Renderer backends (a future
DX11/Vulkan/GL backend binding a swapchain/context to the real window)
need the actual OS handle, which is a fundamentally different kind of
thing: an escape hatch, not a property. It lives in its own sibling
contract resource in `engine.platform`, not a field on `Window`:

```cpp
enum class NativeWindowKind : u8 { Win32, X11, Wayland };

struct NativeWindowHandle
{
  NativeWindowKind kind;
  void* handle;
};
```

`kind` exists because `void*` alone doesn't even disambiguate within one
OS: Linux alone has two native window systems (X11, Wayland).

Whichever platform backend is active inserts `NativeWindowHandle`
alongside `Window`. For `PlatformSDL3Plugin` on Windows, that means
pulling the real `HWND` out of SDL3's window-properties API:
`SDL_GetPointerProperty(SDL_GetWindowProperties(window),
SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)`, tagged
`NativeWindowKind::Win32`. Non-Windows platforms aren't implemented this
pass (hard-fails with a clear message rather than silently returning a
wrong/null handle), follow-up whenever Linux is actually targeted.

This generalizes: a platform backend owns whichever OS resource is
physically singular underneath it (its native window + native event
pump), and populates every contract resource that resource can produce,
not just `Window`. The same reasoning means a future keyboard/mouse
input backend sharing SDL3's event queue would be folded into
`PlatformSDL3Plugin` too, populating `InputState` from the same pump,
rather than a second, independent `SDL_PollEvent` loop racing this one
for the same events. Gamepad (`InputXInputPlugin`) is unaffected either
way: a fully separate OS API, no window or pump involved.

### `PlatformSDL3Plugin`

Same constructor shape as the old win32 stub: `explicit
PlatformSDL3Plugin(WindowDescription description = {})`.

**`build(App& app)`**, synchronous, no `Startup` system needed:

1. `SDL_Init(SDL_INIT_VIDEO)`. On failure: fetch `Diagnostics`, `report()`
   the SDL error (`SDL_GetError()`), `drain_unprinted(print_diagnostic)`
   to flush it immediately, then `assert(false, ...)`, the same
   hard-fail idiom `App::resolve_parameter` already uses for a missing
   required resource. A window that silently fails to open is a worse
   failure mode than a loud, immediate crash with the SDL error attached.
2. `SDL_CreateWindow(description_.title.c_str(), static_cast<int>(description_.width),
   static_cast<int>(description_.height), 0)`. Same failure handling as
   step 1 if it returns null.
3. `insert_resource<Window>({.title = description_.title, .width = description_.width,
   .height = description_.height, .should_close = false})`, the contract
   resource, values copied straight from `description_` (same fields
   `PlatformWin32Plugin` copied from its own `WindowDescription`).
4. `insert_resource<SdlWindowHandle>({.window = sdl_window})`, the
   private resource, `sdl_window` being the pointer `SDL_CreateWindow`
   returned in step 2.
5. `insert_resource<NativeWindowHandle>({...})`, see "Native window
   handle" above.
6. Report "opened window ..." directly via the already-fetched
   `Diagnostics` (`build()` has `App&` already; no need for `Commands`,
   which exists for *systems*, not plugin registration).

**`PreUpdate` system**, `(SdlWindowHandle&, Window&, ExitControl)`: pumps
`SDL_PollEvent` in a loop; on `SDL_EVENT_QUIT` or
`SDL_EVENT_WINDOW_CLOSE_REQUESTED`, sets `Window::should_close = true`,
then calls `exit.request()` if it's set. The win32 stub had this as its
own reactive system, folded into the same one here since both need
`Window::should_close` anyway.

**`Shutdown` system**, `(SdlWindowHandle&, Commands)`: `SDL_DestroyWindow`
+ `SDL_Quit`, then `cmd.report("window closed")`.

Two systems total (`PreUpdate`, `Shutdown`), not three. Window creation
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
- `source/main.cxx`: `PlatformWin32Plugin` -> `PlatformSDL3Plugin`.

## Out of scope (this pass)

- Window resize, focus/minimize, and input events. `Window` has no
  fields for these; adding them is a separate follow-up once needed.
- Multiple windows.
- Actually consuming `NativeWindowHandle` from a renderer backend
  (`engine.renderer.dx11` is unrelated to this change). This pass only
  produces the handle and exposes it; wiring a renderer to it is
  separate follow-up work.
- Non-Windows `NativeWindowHandle` extraction (X11/Wayland). Hard-fails
  with a clear message instead.
- A keyboard/mouse input backend sharing `PlatformSDL3Plugin`'s event
  pump. Noted as the natural extension point, not built this pass.
