# Cricket 07 iOS native port (work in progress)

This directory is an initial iOS controller and architecture foundation, **not a playable port**.

## Design
- SwiftUI application shell, iOS 17+ (planned).
- GameController.framework for supported Bluetooth controllers.
- Objective-C++/C bridge to the existing host input queue (not yet wired).
- Existing translated C runtime compiled for arm64-apple-ios after compatibility validation.
- A diagnostic MetalKit renderer compiles and consumes a bounded synthetic CLEAR/DRAW/PRESENT command stream. It is not a full Direct3D 8 renderer.
- User-owned game assets imported into sandboxed Application Support storage; do not distribute EA assets.

## Current graphics integration (diagnostic only)
- The existing `CmdDraw` triangle-list parser extracts raw Direct3D vertices from the bounded frame queue.
- `cricket07_take_transformed_triangle_frame` applies the reference position transformation from `VertexTransform.c` to each draw before Swift uploads clip-space vertices to Metal.
- The legacy `cricket07_take_triangle_frame` remains available for raw staging and regression tests.
- Unsupported or malformed transforms are rejected rather than rendered as raw clip coordinates. A synthetic mode-2 draw validates half-pixel correction; no live game producer is connected.
- Tests run through `ios-draw-frame-consumer.yml`; `ios-app-shell.yml` compiles and packages the unsigned diagnostic app for iPhone and Simulator.
- Rendering still lacks production texture sampling, per-draw blend/depth/cull state, render targets, game-generated commands, and device gameplay verification.

## Critical blockers
1. Existing build.sh targets macOS and uses Cocoa, IOKit, SDL2, and desktop wgpu libraries.
2. iOS Metal surface and event loop must be implemented and tested.
3. Recompiled runtime needs iOS arm64 memory, floating-point, and exception tests.
4. Native sound, file APIs, and lifecycle handling require platform adapters.
5. Generated translated C sources and user-owned game assets must be supplied to make a playable build.

ControllerBridge.swift queues virtual-key transitions. It does not inject them into the game until the host bridge is implemented. Default bindings are menu navigation only, not verified batting controls.

## Security
- No game assets, signing certificates, tokens, or secrets should be committed.
- Validate imported ZIP paths against traversal, symlinks and decompression bombs.
- Avoid JIT and executable memory on iOS; translated code must be compiled ahead of time.
- Treat game files as untrusted inputs and test parsers before enabling imports.
