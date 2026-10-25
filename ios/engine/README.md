# iOS engine renderer selection (non-playable integration seam)

The engine archive now includes `cricket07_engine_select_ios_recorder()`
and `cricket07_engine_ios_recorder_selected()`. The translated desktop
launcher normally selects the recorder after creating an SDL window, which
cannot be used by the iOS SwiftUI application.

The new function selects the existing `g_render_gpu` recorder explicitly
**only** in `CRICKET07_IOS_GAME + RC_IOS_FRAME_TRANSPORT` builds and only
when its init/present callbacks exist. It is intended to be called on the
future game startup thread **before** creating a Direct3D device. The
recorder then publishes frames through the existing iOS `FrameQueue.c`
mailbox for the Metal consumer. This function does not execute translated
code or initialize Metal.

`ios-engine-renderer-selection.yml` exercises the opt-in, missing-callback,
idempotent and disabled-build cases with synthetic backends. The engine
archive workflow additionally checks the selection symbol and real recorder
backend symbols for both iPhone and Simulator. Neither test proves that
the game can launch or that its frames can be fully rendered.

Still needed: user-owned translated game functions and files, an iOS-safe
engine bootstrap that does not call the desktop `main()`/SDL path,
startup/lifecycle and file-system adapters, complete Direct3D compatibility,
and physical iPhone gameplay tests. Do not package proprietary assets.
