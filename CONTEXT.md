# Domain glossary: chalk / engine

## App

Owns everything: the type-erased **resource** map, the **system** lists per
**Schedule**, and the run loop (`execute()`). Plugins configure an `App`;
systems run against one. Non-copyable, non-movable: one `App` per program
run (or per test).

## Plugin

Anything satisfying `Plugin` (`build(App&)`). A plugin's `build` runs once,
with full `App&` access, to insert resources and register systems. Concrete
plugins are named `<Domain><Backend>Plugin` (`PlatformWin32Plugin`,
`RendererDx11Plugin`): the backend in the name is the adapter; the
domain is the contract it fills (see Contract module, below).

## Resource

A single instance of some type `T`, stored in `App` by type (one per type,
keyed by `type_index`), inserted via `insert_resource<T>`/`Commands::insert_resource<T>`.
Examples: `Time`, `Diagnostics`, `Window`, `InputState`, `PhysicsWorld`.

## System

A callable registered via `App::add_system(Schedule, callable)`. Its
parameter list *is* its dependency declaration: `App` inspects the
callable's signature and resolves each parameter before calling it:

- `T&`: required resource. Missing at run time asserts (type name +
  Schedule + resource name printed first). Use when the system cannot do
  anything meaningful without it.
- `Option<T>`: optional resource (by value). Missing is a checked `bool`,
  not a crash. Use when the system degrades gracefully.
- `Commands` (by value): narrow capability, insert/remove a resource,
  report a diagnostic. For systems that only need to cause those specific
  side effects.
- `ExitControl` (by value): narrow capability, `request()` to stop the
  `App`'s run loop. Nothing else.
- `App&`: full access. Reserved for `Plugin::build` and `CorePlugin`'s own
  bootstrap system, not for systems in other plugins.

`Commands` and `ExitControl` live in `engine.command` (`engine/command/`),
not `engine.core`. `App::resolve_parameter` never names them: it resolves
any type constructible from `App&` this way, so `engine.core` has no
dependency on `engine.command` (only the reverse). `engine.command`'s
primary module interface (`command.ixx`) re-exports partitions:
`:commands` (the `Commands` class shape), `:insert_resource`,
`:remove_resource`, `:report` (each defines one `Commands` member
out-of-line), and `:exit` (`ExitControl`, self-contained).

## Schedule

The six phases a system can run in, in this order every frame:
`Startup` (once) → `PreUpdate` → `Update` → `PostUpdate` → `Render` →
(loop) → `Shutdown` (once, after the loop ends).

## Option\<T\>

Non-owning, possibly-empty reference to a resource, with `operator->`/
`operator*`/`operator bool` so call sites read `option->field` directly.
Wraps `std::optional<std::reference_wrapper<T>>`. What we actually want
is `std::optional<T&>` (C++26): rebinds on assignment rather than
assigning through, exactly the semantics a resource reference needs. Not
available here: this toolchain's standard library (MSVC STL, picked up
by clang++ by default on Windows regardless of compiler choice) doesn't
implement the C++26 reference specialization yet, and its fallback,
`std::optional<std::reference_wrapper<T>>`, only gives you the
`reference_wrapper` on dereference (`option->get().field`), not `T`
directly, and that can't be fixed by overloading `std::optional`'s own
`operator->` (member-only, not our type to extend). `Option<T>` is the
thin wrapper that restores the ergonomics; if libc++ (or an MSVC STL
update) ever adds `std::optional<T&>` here, swap what `Option<T>` wraps
internally and every call site keeps working unchanged.

`is_option<T>`/`IS_OPTION_V<T>` (in `engine.core`'s `:option` partition,
`engine/core/core.option.ixx`, alongside `Option<T>` itself) let
`App::resolve_parameter` tell an optional system parameter from a
required one.

Previously named `Handle<T>` (before that `Ref<T>`), renamed because
Bevy has its own, unrelated `Handle<T>` (an asset reference, never itself
empty), colliding with this codebase's Bevy-derived vocabulary. Briefly
removed in favor of raw `std::optional` directly, then reinstated once
that turned out to cost the `operator->` ergonomics.

## Contract module

A module that exports only the shared data a **domain** (input, physics,
eventually renderer, audio) hands to consumers, with no backend code.
`engine.input` (`InputState`) and `engine.physics` (`PhysicsWorld`) do
this today; a backend module (`engine.input.xinput`,
`engine.physics.simple`) fills the contract. `engine.renderer` and
`engine.audio` are reserved for this but deliberately left empty until a
second backend of either exists, see
[docs/adr/0001-defer-renderer-audio-contracts.md](docs/adr/0001-defer-renderer-audio-contracts.md).

## Diagnostics

The one logging mechanism: every plugin's status messages ("[platform]
opened window...", "[physics] step N...") and every engine-internal
warning route through it via `Commands::report`/`Diagnostics::report`,
rather than any subsystem printing to `std::cout` directly. There used to
be two paths (ad hoc `std::cout <<` in plugins, plus `Diagnostics` for
occasional warnings); unified into one on request.

Each `report()` call captures a `std::source_location` (defaulted to the
call site) into a `DiagnosticMessage{text, location}`, buffered in
`Diagnostics` and drained once per `PreUpdate` via `drain_unprinted`,
printed by `print_diagnostic` (`engine.core`, `core.ixx`) as
`[tag] file:line: text`, tag leftmost. The tag is never typed by the
caller: `tag_from_file_name` (`core.option.ixx`, alongside
`short_file_name`) takes the segment before the first `.` in the
source file's name; every file here is named `<tag>.<backend?>.ixx`
(`renderer.dx11.ixx`, `platform.win32.ixx`, `core.ixx`), so the tag falls
out of the existing naming convention for free, no hardcoded plugin list
to keep in sync. The drain cursor lives inside `Diagnostics` itself, not
in the draining system, so two `App` instances never share it.

`print_diagnostic` flushes explicitly (`std::endl`, not `'\n'`):
buffered `std::cout` can be lost entirely when the missing-required-resource
path prints one right before `assert()` aborts (observed when stdout is
piped, not a tty).

One case can't wait for the scheduled drain: `App::resolve_parameter`'s
missing-required-resource path calls `assert()` right after reporting,
which aborts before the next `PreUpdate` would ever run. That path calls
`diagnostics->drain_unprinted(print_diagnostic)` directly, flushing
synchronously: same `Diagnostics` API, just triggered immediately
instead of on schedule, so there's still exactly one reporting mechanism,
not two.
