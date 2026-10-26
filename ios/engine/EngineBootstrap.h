#ifndef CRICKET07_ENGINE_BOOTSTRAP_H
#define CRICKET07_ENGINE_BOOTSTRAP_H

#include "../../runtime/runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Prepare the translated host on the single engine startup thread.
 * Installs the existing Win32/DirectX import handlers and selects the iOS
 * frame recorder, in that order. Returns 1 only when both succeed.
 * Repeated successful calls are no-ops. A failed call may have registered
 * imports, but never marks the host ready; retry is allowed.
 *
 * This does NOT allocate guest memory, load a PE, register generated game
 * functions, launch SDL, create a Metal surface or execute any game code.
 * Do not call from Swift UI callbacks or the Metal rendering thread.
 */
int cricket07_engine_prepare_host(void);
int cricket07_engine_host_prepared(void);

/* After host preparation, reserve guest memory and initialize the persistent
 * CPU state on the same startup thread. Failure is recoverable and retryable.
 * This does not load or execute the game. */
int cricket07_engine_prepare_guest(void);
CPU *cricket07_engine_guest_cpu(void); /* NULL until successful */

/* Explicit, non-executing image-load seam for a user-owned PE32 file.
 * Accepts only absolute iOS paths. Initializes the guest if necessary,
 * then invokes the validated iOS loader once on the single startup thread.
 * Failed loads leave *out unchanged and may be retried; a successful load
 * blocks further image loads for this guest lifetime. No translated
 * functions are registered or called by this wrapper. Never call from
 * the SwiftUI or Metal rendering threads. Not a security sandbox.
 */
int cricket07_engine_load_user_executable(const char *absolute_path, LoadedImage *out);
int cricket07_engine_image_loaded(void);

/* Non-executing gate for a future translated-game startup. Returns 1 only
 * after a successful image load, when its entry address lies within that
 * image and has a generated function registered in the runtime. On failure
 * neither output is modified. Does not register, invoke, or initialize
 * translated game functions. Call only on the single engine startup thread.
 * A successful result is not proof that the game can run or render.
 */
int cricket07_engine_resolve_loaded_entrypoint(GuestFn *out_fn, uint32_t *out_address);

/* One-shot handoff for a separately linked generated register_all_functions()
 * callback. Call after loading a user-owned PE on the single engine thread.
 * The callback must only populate the translated function registry. It is
 * invoked at most once, even if it fails to register the image entry point,
 * because the runtime registry cannot roll back partial registrations.
 * Success verifies only the entry point, not complete game coverage.
 * This API does not call the translated entry point or start gameplay.
 */
int cricket07_engine_install_translated_functions(void (*registrar)(void));
int cricket07_engine_translated_functions_installed(void);

/* Compile-time gate for a generated register_all_functions() symbol.
 * The default diagnostic build returns 0 without a symbol reference.
 * Build with CRICKET07_LINKED_TRANSLATED_REGISTRY=1 only when the generated
 * translation objects are explicitly linked into the final application.
 * Linking without those objects will correctly fail at link time.
 */
int cricket07_engine_linked_translated_functions_available(void);

/* Install the explicitly linked generated registry using the existing
 * one-shot registration gate. No translated game function is executed.
 * The loaded executable and guest must already be prepared on the engine
 * startup thread. Disabled builds leave registration retryable.
 */
int cricket07_engine_install_linked_translated_functions(void);

#ifdef __cplusplus
}
#endif
#endif
