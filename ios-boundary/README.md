# Boundary 07 for iOS

This is the open-source iOS host for the locally generated Boundary 07 WebAssembly build. The app imports a private folder through Files, verifies the runtime and executable hashes, and serves its resources only through an on-device loopback origin. It never uploads game inputs to GitHub.

The host targets iOS 26 and landscape iPhone/iPad. Version 0.2.0 (3) uses a pointer-capture touch controller with independent finger ownership. The upstream browser Gamepad API remains available for physical controllers, subject to device testing.

## Touch controls

- The left stick sends eight keyboard directions, including two held arrow keys for diagonals. Keep it held while pressing a face button with another finger. The original game receives keyboard directions rather than proportional analog axes.
- Xbox-style face layout: A = S, B = D, X = A, Y = W. Small letters show the original keyboard bindings. Assign the keyboard player to your team in the game.
- LOFT toggles Shift on or off; it stays held through shot presses. START sends Escape, SELECT sends Tab. These are separate buttons.
- Drag the clear middle area as a relative mouse trackpad. Lift and drag again to move farther; a short stationary tap clicks the left mouse button. This does not require browser pointer lock.
- Screen: Fit keeps the game's 4:3 proportions. Fill stretches it across the display. Quality: Performance = 960×720, Balanced = 1280×960 (default), Sharp = 1600×1200. Settings persist locally. Viewport sizing follows the visible screen, and controls respect safe areas.
- Hide controls releases every held touch input and returns direct canvas interaction for external input. Backgrounding, interrupted touches, dialogs and leaving the game release inputs and switch Loft off.

Update the same installed app using the same Sideloadly signing identity and bundle identifier to retain its private imported files. This host-only update needs no replacement game bundle.

## Install and import

1. Download the `Boundary07-iOS-Host` artifact from the **Boundary 07 iOS host** workflow. Its IPA has an ad-hoc signature and must be re-signed/provisioned locally before an iPhone will install it.
2. Transfer the private `Boundary07-iOS-Private` folder from the Windows outputs to Files on the phone. A ZIP may be expanded using Files. This folder contains proprietary game-derived files and must remain private.
3. Open Boundary 07 and choose **Import private bundle**. Select the folder with `bundle.json`, `Runtime`, and `Game`, not an enclosing folder.
4. Choose **Check this device**. Export its report if a required feature is unavailable.
5. If checks pass, choose **Open the pavilion**, allow the first private-storage import to finish, and choose **Play cricket**.
6. Use S to advance the initial prompt. On team selection, assign the keyboard player to a team with the arrows. Check menus, a real match, sound, controls, saves and sustained memory behavior before claiming playability.

The first import keeps an app-private source copy and an OPFS copy, in addition to any ZIP/folder retained in Files. Allow several GB of free space. The upstream guest alone allocates 1.25 GiB plus stacks, graphics and host memory. Feature probes deliberately avoid allocating the full guest region; a passing probe cannot establish that it fits on a physical iPhone.

## Runtime gates

The host checks secure context, cross-origin isolation, shared WebAssembly memory and compilation, actual WebGPU command submission, writable OPFS, worker shared memory, and synchronous worker OPFS access. Missing requirements block game launch. These are tested in the actual app context; Safari support alone is insufficient.

Only public WebKit APIs are used. ATS remains enabled; the only networking exception is `NSAllowsLocalNetworking`. The server binds IPv4 loopback, requires an installation token, rejects path traversal, streams large files, and sends COOP/COEP/CORP headers. The web view only navigates within the local origin and token path; no remote asset or game-code origin is configured.

The Windows-generated `recomp.js`/`recomp.wasm` are privately imported at runtime. This build does not claim the Evolved-style native AOT fallback. If WKWebView lacks a required feature or cannot sustain the game, the native host for Emscripten/WebGPU/audio/files/threads is still separate engineering work.

## CI evidence

The workflow compiles a physical-device app, tests synthetic private-bundle imports (including corrupted inputs and links), checks local server streaming and headers, records a simulator WKWebView capability report, and exercises concurrent controller inputs against fake engine exports in WebKit. No real game files are used in CI. An IPA build, passing unit tests, or a simulator report does not establish gameplay on iPhone 17. Simulated pointer tests do not replace physical multitouch checks on the phone.

The original Cricket 07 menus remain EA game content. Boundary 07 supplies the custom launcher and host. Upstream MIT attribution is preserved under `Resources/Web/LICENSE.txt` and About.
