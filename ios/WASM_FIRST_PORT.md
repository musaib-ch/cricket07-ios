# Cricket 07 WebAssembly-first iOS port

## Decision
The WebAssembly build is the source of truth for game execution. The existing native foundation is retained only as a fallback. Do not treat diagnostic Metal frames or a successfully linked shell as playable gameplay.

## Existing pipeline
The repository already translates the user's Cricket 07 x86 executable into generated C with `build.sh`, then builds a threaded Emscripten WebAssembly application with `build_web.sh`. This step requires the user's legally obtained game executable, a Ghidra function map, and generated `out_all/*.c`; these files are deliberately absent from the public repository.

## First playable candidate: bundled offline WebAssembly runtime
1. Generate the real game build in a private environment: `PROFILE=work/profile.h ./build.sh /private/game.exe work/functions.json` followed by `PROFILE=work/profile.h ./build_web.sh`.
2. Package the browser output `build_web/www/recomp.js`, `recomp.wasm`, worker scripts, and `web/` loader resources into an iOS app bundle. Never fetch game code from an untrusted URL at runtime.
3. Prototype a landscape `WKWebView` host loading bundled content via a local HTTP origin, not `file://`. The existing web loader uses WebGPU, SharedArrayBuffer, pthreads, OPFS, and workers. Verify these features on the target iOS/WebKit version before attempting game startup. A local HTTP origin may not satisfy cross-origin isolation; test `crossOriginIsolated`, `navigator.gpu`, `SharedArrayBuffer`, and OPFS explicitly. A missing capability blocks this route.
4. Import the user's game assets into private app storage. The browser loader currently expects OPFS; a bridge or a one-time OPFS import is required.
5. Connect SwiftUI touch buttons and GameController events to browser input using a narrowly scoped JavaScript bridge.
6. Test loading the actual menu and a match on an iPhone. Record FPS, memory pressure, audio, graphics, and controller behavior.

## Evolved-style fallback: ahead-of-time native compilation
If WebKit cannot meet the browser build's thread, memory, WebGPU, or OPFS requirements, translate the **existing generated WebAssembly** to C with `wasm2c` and compile ARM64 at build time. Implement a native host for Emscripten imports, threads, files, sound, and WebGPU-on-Metal. This is substantial engineering; wasm2c alone cannot turn the browser build into a working native iOS application.

## Privacy and reproducibility
- Never commit proprietary game executable, generated game code, assets, or a proprietary wasm bundle to this public repository.
- Use private, access-controlled build inputs for any game-derived binary.
- Publish only the open-source app shell and test harness unless distribution rights are established.
- GitHub Actions compilation success is not evidence of playable game execution.

## Gates
A. Browser-generated wasm artifact is produced from authorized game inputs.
B. Device capability probe confirms required WebKit features or forces native-AOT fallback.
C. Game runtime reaches first frame and main menu.
D. Controller/touch input works in menu and match.
E. Audio, assets, save/load, and sustained gameplay pass device tests.
