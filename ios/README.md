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
- The viewport-aware Metal consumer now preserves individual `CMD_DRAW` ranges and translates logical Direct3D viewports into drawable-pixel Metal viewports, including bounds clipping and depth-range validation. The synthetic producer includes an explicit backbuffer `CMD_TARGET`.
- The legacy `cricket07_take_triangle_frame` remains available for raw staging and regression tests.
- Unsupported or malformed transforms are rejected rather than rendered as raw clip coordinates. A synthetic mode-2 draw validates half-pixel correction; no live game producer is connected.
- Tests run through `ios-draw-frame-consumer.yml`; `ios-app-shell.yml` compiles and packages the unsigned diagnostic app for iPhone and Simulator.
- The diagnostic Metal renderer now translates per-draw depth comparison/write, culling, RGBA write masks and a bounded set of D3D8 blend factors/operations into cached Metal pipeline states.
- The unlit diagnostic draw path now decodes each `CVert.diffuse` ARGB value into normalized RGBA and uploads position+color pairs to Metal. Textured, lit, and pixel-shader draws are explicitly rejected by this path; this is not game shading. Unsupported values fail validation. Production texture sampling, alpha testing, non-backbuffer render targets, game-generated commands, and device gameplay verification remain outstanding.
- `TextureUploadBridge.c` validates bounded RGBA8 mip-chain commands, and `MetalTextureStore.swift` now allocates and uploads those mip levels into a memory-capped Metal texture cache. A synthetic 2x2 test texture exercises the upload during diagnostic initialization. **No texture is bound to a draw, no game texture commands are consumed, and texture sampling is not implemented.**

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
