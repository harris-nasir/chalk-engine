# 0001: Defer renderer/audio contract modules

## Status

Accepted (2026-08-02)

## Context

`engine.input` and `engine.physics` each split into a contract module
(`InputState`, `PhysicsWorld`) separate from their backend module
(`engine.input.xinput`, `engine.physics.simple`). `engine.renderer` and
`engine.audio` don't follow that split: both are empty modules; DX11 and
XAudio2 are the only seam.

An architecture review flagged this as an inconsistency worth fixing:
define a render-target-style resource in `engine.renderer` and a
mixer/voice-style resource in `engine.audio`, mirroring the input/physics
pattern.

## Decision

Defer. DX11 and XAudio2 are the only backend of each kind that exists. The
review's own heuristic (one adapter is a hypothetical seam, two adapters
make it real) applies directly here (unlike the Commands/ExitControl
question from the same review, which was about narrowing a capability
surface, not justifying a seam behind multiple implementations). Guessing
the contract's shape before a second backend (Vulkan, WASAPI, ...) exists
means guessing what that seam needs to carry.

Get the DX11/XAudio2 backend fully built out first. Extract the contract
when a second backend is actually being built, using the first backend's
usage as the evidence for what belongs in it.

The empty `engine.renderer`/`engine.audio` modules stay as-is (reserved
seam, not yet filled), so nothing here blocks adding the contract later.

## Consequences

- A future architecture review should not re-flag this as unaddressed
  inconsistency; it's an intentional deferral, not an oversight.
- Re-open this decision when a second renderer or audio backend is
  actually planned.
