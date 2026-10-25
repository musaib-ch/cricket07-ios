# Cricket 07 iOS native port (work in progress)

This directory is an initial iOS controller and architecture foundation, **not a playable port**.

## Design
- SwiftUI application shell, iOS 17+ (planned).
- GameController.framework for supported Bluetooth controllers.
- Swift GameController transitions now enter a bounded, mutex-protected C HID event queue. The translated host input consumer is not linked yet.
- Existing translated C runtime compiled for arm64-apple-ios after compatibility validation.
- A diagnostic MetalKit renderer compiles and consumes a bounded synthetic CLEAR/DRAW/PRESENT command stream. It is not a full Direct3D 8 renderer.
- User-owned game assets imported into sandboxed Application Support storage; do not distribute EA assets.

## Current graphics integration (diagnostic only)
- The existing `CmdDraw` triangle-list parser extracts raw Direct3D vertices from the bounded frame queue.
- `cricket07_take_transformed_triangle_frame` applies the reference position transformation from `VertexTransform.c` to each draw before Swift uploads clip-space vertices to Metal.
- The viewport-aware Metal consumer now preserves individual `CMD_DRAW` ranges and translates logical Direct3D viewports into drawable-pixel Metal viewports, including bounds clipping and depth-range validation. The synthetic producer includes an explicit backbuffer `CMD_TARGET`.
- The legacy `cricket07_take_triangle_frame` remains available for raw staging and regression tests.
- Unsupported or malformed transforms are rejected rather than rendered as raw clip coordinates. A synthetic mode-2 draw validates half-pixel correction; no live game producer is connected.
- Tests run through `ios-draw-frame-consumer.yml`; `ios-app-shell.yml` compiles and packages the unsigned diagnostic app for iPhone and Simulator.
- The diagnostic Metal renderer now translates per-draw depth comparison/write, culling, RGBA write masks and a bounded set of D3D8 blend factors/operations into cached Metal pipeline states.
- The unlit diagnostic draw path now decodes each `CVert.diffuse` ARGB value into normalized RGBA and uploads position+color pairs to Metal. Textured, lit, and pixel-shader draws are explicitly rejected by this path; this is not game shading. Unsupported values fail validation. Production texture sampling, alpha testing, non-backbuffer render targets, game-generated commands, and device gameplay verification remain outstanding.
- `TextureUploadBridge.c` validates bounded RGBA8 mip-chain commands, and `MetalTextureStore.swift` allocates those mip levels into a memory-capped Metal texture cache. The synthetic 2x2 texture is now emitted as `CMD_UPLOAD_TEX` inside the same queued frame as the diagnostic triangle. The C consumer validates/copies it and Swift uploads it to Metal before binding the texture to the draw. The texture store starts empty; the diagnostic no longer bypasses the recorder command stream. No game texture producer or engine execution is connected.

- The diagnostic `CMD_DRAW` path carries bounded, finite `CVert.tc[0].xy` values to Metal as explicit UV0 attributes. The generated triangle uses recorded UVs for sampling instead of deriving them from clip-space positions.
- The UV-enabled consumer now preserves a validated stage-0 texture ID per draw. It accepts only D3DTOP_MODULATE (texture multiplied by diffuse/current), nearest filtering with clamp U/V, stage 1 disabled, and no pixel shader, lighting, alpha test or specular addition. Metal binds a resident texture per draw and skips draws with missing texture IDs instead of silently rendering them as untextured. The synthetic diagnostic frame exercises this path using generated texture ID 1. The opt-in frame parser now validates and returns `CMD_UPLOAD_TEX`/`CMD_DESTROY_TEX` mutations that occur **before the first draw**; the Metal view applies them before rendering. Interleaved texture mutations, unsupported texture formats/stages and other Direct3D8 modes remain unsupported. No live game producer or playable engine is connected.

## Critical blockers
1. Existing build.sh targets macOS and uses Cocoa, IOKit, SDL2, and desktop wgpu libraries. The opt-in iOS recorder now avoids desktop GPU initialization and refuses synchronous readback rather than waiting indefinitely; it still cannot perform readbacks and is not linked to a game executable.
2. iOS Metal surface and event loop must be implemented and tested.
3. Recompiled runtime needs iOS arm64 memory, floating-point, and exception tests.
4. Native sound, file APIs, and lifecycle handling require platform adapters.
5. Generated translated C sources and user-owned game assets must be supplied to make a playable build.

ControllerBridge.swift queues virtual-key transitions and FoundationView forwards them to ControllerInputQueue.c. The queue maps arrow keys, Enter and Escape to the same USB HID usages understood by host/input.c, deduplicates held transitions, and clears potentially stuck keys on overflow, view teardown or reset. ios-controller-input-queue.yml exercises mapping, release, and overflow behavior. A future game-thread adapter must drain Cricket07HIDEvent values and call input_key_hid() on the translated host thread. Events are NOT injected into the game yet. Default bindings are menu navigation only, not verified batting controls.

## Security
- No game assets, signing certificates, tokens, or secrets should be committed.
- Validate imported ZIP paths against traversal, symlinks and decompression bombs.
- Avoid JIT and executable memory on iOS; translated code must be compiled ahead of time.
- Treat game files as untrusted inputs and test parsers before enabling imports.

## Game-thread controller dispatch (diagnostic adapter)
- `ControllerInputPump.c` provides `cricket07_controller_pump(sink, context, budget)` to drain up to 32 HID transitions per invocation and invoke a caller-provided callback outside the queue mutex. A translated host input-thread callback should forward each transition to `input_key_hid(usage, pressed)`; calling it from a Swift controller callback or the Metal render thread would be incorrect.
- `ios-controller-input-pump.yml` tests FIFO order, bounded consumption, cross-thread dispatch, reentrant enqueue and key releases with a stub sink. The current SwiftUI diagnostic shell **does not call this pump** and does not execute the translated game. This is an integration seam, not verified DirectInput gameplay.

## DirectInput adapter (not yet linked to gameplay)
- `ios/engine/ControllerDirectInputAdapter.c` implements `cricket07_controller_pump_directinput(budget)`, forwarding the existing bounded HID queue to the real `host/input.c:input_key_hid()` symbol. There is deliberately no fallback implementation.
- `ios-controller-directinput-adapter.yml` compiles the queue, pump, adapter and a strict link-time `input_key_hid` stub. It verifies FIFO transitions, 32-event maximum, game-thread callback execution, empty-queue behavior and release-on-reset.
- The adapter is outside the diagnostic Xcode source target so that it cannot introduce an unresolved `input_key_hid` symbol into the IPA. When a translated game executable and host input system are linked, add this source to the game target and invoke it **only on the game input thread** at a defined event-loop checkpoint.
- Passing these tests does not demonstrate a running engine, DirectInput gameplay, or device controller compatibility.
