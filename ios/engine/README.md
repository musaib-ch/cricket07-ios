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


## iOS immutable import staging

The iOS loader now builds a bounded `Cricket07PEImportPlan` from validated,
immutable PE file bytes before it writes the image to guest memory. The plan
captures each import's resolved `DLL!function` name (or ordinal) and IAT
destination RVA. This avoids reading names or thunk metadata from guest memory
after earlier IAT writes might have overwritten them, including PE files where
`OriginalFirstThunk == FirstThunk`. The desktop loader is unchanged.

`ios_pe_import_plan_test.c` exercises normal imports, ordinal imports,
IAT/ILT aliasing, exhausted slot capacity, and malformed import metadata
using synthetic images under ASan/UBSan. The engine-library workflow compiles
the plan and verifies the iOS runtime references it.

**Remaining safety work:** This is staged *metadata*, not an atomic image
transaction. Guest-memory section writes, import registrations, and IAT writes
still need rollback/retry guarantees, and runtime dynamic import registration
must be bounded. Do not expose arbitrary-file loading or claim gameplay.

## Bounded dynamic imports and COM slot safety

`add_dynamic_import()` now rejects NULL/empty/overlong names, NULL callbacks,
and exhausted import slots without consuming a slot or returning a forged
dispatch address. The function table now tolerates allocation failure and
bounds hash probing. `runtime_import_slots_remaining()` allows COM vtables to
check capacity and method-name lengths before registering any methods.
`build_vtable()` and `com_new()` reject failures instead of treating the
zero failure sentinel as an import-table index.

`ios-dynamic-imports.yml` exercises the real runtime registration API,
including the 4096-slot boundary, under ASan/UBSan using synthetic names.
This does not establish full transactional rollback for executable loading
or for an unexpected mid-vtable allocation failure. Game startup, Metal
compatibility, and physical-device gameplay are still unverified.

## Private PE image staging (synthetic CI only)

The iOS-only `load_image()` now validates PE metadata, stages import names,
checks dispatch capacity, and builds a fully patched, zero-initialized PE image
in private heap memory before copying it into guest memory. Failed allocation
or validation leaves guest image bytes, import slots and the output image
metadata unchanged. Duplicate or partially overlapping IAT patch addresses
are rejected, and loading a second executable into a live guest is refused.
The desktop loader is unchanged.

`ios-pe-image-stage.yml` runs synthetic image/section/IAT and failure-path
tests under ASan/UBSan. Both iPhone and Simulator engine archives compile
`PEImageStage.c` and verify the runtime references it. No game files are
distributed.

**Scope:** The final commit assumes a single engine loader thread, with no
concurrent dynamic import registration between preflight and publication.
It is not a general-purpose multithreaded transaction or a sandbox for
arbitrary executables. The private image can temporarily increase memory
pressure, so a physical-device test is still required. Generated game
functions, iOS startup/lifecycle, actual Metal frames and gameplay remain
unconnected and unverified.

## Controlled, non-executing engine image-load seam

`cricket07_engine_load_user_executable()` now accepts an absolute path to a
user-owned executable on the dedicated engine startup thread. It validates
its arguments before initializing the guest, reuses the existing host/CPU
bootstrap, calls the iOS-only staged PE loader, and preserves caller output
metadata on load failure. The image-loaded guard prevents a second load in
the same guest lifetime. Failures before publication can be retried. The
bootstrap regression now tests invalid paths, guest-allocation failure,
loader failure, retry and duplicate-load rejection without game assets.

This is **not wired into the SwiftUI app**, does not register generated game
functions or execute the entry point, and is not a security boundary for
arbitrary executables. Device memory pressure, imports, Metal rendering,
controller behavior during gameplay, and gameplay itself remain unverified.

## Non-executing translated entrypoint resolution

`cricket07_engine_resolve_loaded_entrypoint()` checks that the single loaded
image's entry address is inside its published guest image range and that the
runtime registry already contains a translated callback at that address. It
returns the callback and entry address without invoking either. It fails
without modifying output arguments if no image has loaded, the metadata is
inconsistent, or generated code has not been registered. The bootstrap CI
covers absent functions, invalid output pointers, stale image bounds, and
successful synthetic resolution; both iOS engine archives check the symbol.

**Still missing:** generated Cricket 07 functions are not part of the current
iOS archive or diagnostic IPA. This gate deliberately cannot create them,
start gameplay, validate their completeness, or prove Metal/DirectInput
compatibility. Use only on the dedicated engine thread, not from SwiftUI.
