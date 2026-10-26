#ifndef CRICKET07_ENGINE_LAUNCH_GATE_H
#define CRICKET07_ENGINE_LAUNCH_GATE_H

#include "../../runtime/runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CRICKET07_LAUNCH_INVALID_ARGUMENT = 0,
    CRICKET07_LAUNCH_ALREADY_STAGED,
    CRICKET07_LAUNCH_REGISTRY_UNAVAILABLE,
    CRICKET07_LAUNCH_STAGE_FAILED,
    CRICKET07_LAUNCH_REGISTRATION_FAILED,
    CRICKET07_LAUNCH_ENTRY_UNRESOLVED,
    CRICKET07_LAUNCH_READY
} Cricket07LaunchStatus;

/* Pre-execution handoff for a future dedicated engine thread. Requires a
 * separately linked generated registry and user-owned game files. Refuses
 * to load an image when the registry is absent, preventing an unusable
 * one-shot image publication in the diagnostic configuration.
 *
 * READY returns the translated entry callback and address WITHOUT invoking
 * it. A staging adapter must publish a nonzero, coherent image before
 * one-shot registration is attempted. Output arguments are unchanged on
 * every failure. Once staging
 * succeeds, subsequent registration/resolution failures leave the image
 * loaded and must NOT be retried in the same process. This function is
 * not a sandbox, a renderer initialization, or proof of playable gameplay.
 * Never call it from the SwiftUI/Metal threads.
 */
Cricket07LaunchStatus cricket07_engine_prepare_launch(
    const char *game_directory, const char *user_directory,
    const char *executable_path, LoadedImage *out_image,
    GuestFn *out_entry, uint32_t *out_entry_address);

#ifdef __cplusplus
}
#endif
#endif
