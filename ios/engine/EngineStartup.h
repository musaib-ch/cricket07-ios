#ifndef CRICKET07_ENGINE_STARTUP_H
#define CRICKET07_ENGINE_STARTUP_H

#include "../../runtime/runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Single-threaded, non-executing startup staging for user-owned game files.
 * Canonicalizes the game directory and executable, requires a readable
 * regular executable within the game directory (not a sibling or a symlink
 * resolving outside it), configures separate game/user roots, then invokes
 * the existing staged PE loader. No translated function is registered or
 * executed, and no Metal/SDL lifecycle is started.
 *
 * Invalid paths leave output and engine paths untouched. If PE loading fails,
 * configured paths persist and a retry with the same roots is allowed.
 * The filesystem may change between validation and open; this is NOT a
 * security sandbox or authorization to run untrusted executables.
 * Only call on the dedicated engine startup thread.
 */
int cricket07_engine_stage_user_game(const char *game_directory,
                                    const char *user_directory,
                                    const char *executable_path,
                                    LoadedImage *out);

#ifdef __cplusplus
}
#endif
#endif
