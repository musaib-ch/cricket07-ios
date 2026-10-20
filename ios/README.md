# Cricket 07 iOS native port (work in progress)

This directory is an initial iOS controller and architecture foundation, **not a playable port**.

## Design
- SwiftUI application shell, iOS 17+ (planned).
- GameController.framework for supported Bluetooth controllers.
- Objective-C++/C bridge to the existing host input queue (not yet wired).
- Existing translated C runtime compiled for arm64-apple-ios after compatibility validation.
- Metal graphics via an iOS-compatible wgpu-native surface or an alternative renderer (not yet implemented).
- User-owned game assets imported into sandboxed Application Support storage; do not distribute EA assets.

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
