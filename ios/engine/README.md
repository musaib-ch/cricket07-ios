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

## Shared SDL-free startup preparation

The desktop launcher now calls `host_register_import_handlers()` instead of
assembling the six Windows/DirectX import tables inline. The new shared
`host/import_registry.c` module validates the complete table count against
its 1024-entry bound and rejects missing lists **before** installing any
handlers. The iOS engine archive includes the same implementation, making
the import-registration step reusable from a future native startup thread
without invoking `main()`, SDL, or the game.

`ios-engine-import-registry.yml` tests ordering, idempotent re-registration,
oversized tables, and missing table pointers. This is a startup prerequisite,
not an engine launch: image loading, guest memory initialization, generated
function registration, iOS file-system setup and lifecycle remain unconnected.

## Recoverable guest-memory initialization (not game startup)

`runtime_try_init(CPU *)` now reserves the guest address space and initializes
the CPU/TIB without calling `exit()` if the reservation fails. The original
`runtime_init()` preserves its fatal launcher behavior. Failed reservations
leave the CPU and global guest pointer unchanged; repeat initialization is
rejected to avoid replacing a live mapping. The optional
`RC_TEST_GUEST_MAP_FAILURE` compile flag exercises the failure path in CI.

`cricket07_engine_prepare_guest()` runs the existing host preparation first,
then initializes a persistent engine-owned CPU. Its accessor returns NULL until
the guest is ready, and successful preparation is idempotent. All calls must
remain on the single future engine startup thread. This is a prerequisite for
loading the user's executable, not a game launch. On iPhone, successful virtual
reservation and actual memory pressure must still be verified on device.

## Bounded PE32 preflight (validation only)

`ios/engine/PEPreflight.c` provides `cricket07_pe32_preflight()` to
check a caller-owned byte buffer before any guest memory writes. It rejects
truncated DOS/PE/section headers, non-x86 or PE32+ images, implausible
image and section bounds, overlapping sections, invalid raw ranges, and
entry points outside an executable section. It requires a non-empty import
directory because the existing `load_image()` assumes one. On success it
returns only basic image metadata; on failure it leaves output untouched.

`ios-pe-preflight.yml` runs synthetic fixture tests under ASan/UBSan. The
iPhone and Simulator static engine archives compile the preflight module,
but the SwiftUI diagnostic app does not call it and no PE is loaded.
**This is not a safe loader**: `runtime/runtime.c:load_image()` still trusts
nested import descriptors, names and thunk chains, performs unchecked guest
writes, and has no bounded error path. Before calling it with user files,
validate all import-table and RVA-to-file mappings and make loading
recoverable. The caller must enforce the 128 MiB file cap before reading a
file into memory. No proprietary executable is included or tested.

## PE import metadata validation (read-only)

`cricket07_pe32_imports_preflight()` now validates the PE import
descriptor terminator, DLL names, import lookup/address table spans,
hint/name strings, ordinal imports, and a 4096-entry import bound.
It maps RVAs to actual raw file bytes, rejecting references that exist
only in zero-filled virtual section tails. Failures leave the caller's
import count unchanged. The new `ios-pe-imports.yml` regression uses
synthetic fixtures under AddressSanitizer and UndefinedBehaviorSanitizer.
Both native engine archives compile the validator.

**Do not pass untrusted executables to `load_image()` yet.** The legacy
loader is still unchecked, does not call these validators, and can
abort or corrupt guest memory on malformed data. Its import writes may
alias other image structures, and loader-side bounds, error recovery,
and a controlled startup call remain required. No gameplay claim.

## iOS PE load gate wired into runtime (still not a playable loader)

The iOS engine now compiles `PELoadGate.c` and invokes
`cricket07_pe32_load_gate()` from `runtime/runtime.c:load_image()`
**before any guest image writes**. It combines the existing PE32 and
import-table preflights and rejects an import count exceeding the remaining
slots in the fixed 4096-entry dispatch table. This gate is compiled only
for `CRICKET07_IOS_GAME`, leaving desktop loading behavior otherwise
unchanged. File reading is capped at 128 MiB, failures clean up allocations,
and image-size metadata is captured before the file buffer is freed
(fixing a prior use-after-free). Import insertion also refuses to index
beyond the fixed slot table.

`ios-pe-load-gate.yml` tests the gate with synthetic PE fixtures under
ASan/UBSan, including a full import table and malformed image/import
metadata. The engine archive workflow verifies that the iOS runtime
references the gate and the archive defines it.

**Not yet safe to expose as an arbitrary-file import UI.** Import
resolution is not transactional, so overlapping IAT and import-name
metadata can still corrupt later resolution. The runtime must stage
validated import names and IAT writes before mutation, add recovery from
partial loading, and test guest image loading with synthetic executables.
The SwiftUI shell does not call the loader or execute game code. Game
assets are never included in CI.
