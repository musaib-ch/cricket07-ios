#ifndef CRICKET07_ENGINE_GAME_PATHS_H
#define CRICKET07_ENGINE_GAME_PATHS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Configure the native host's game-data and writable user-data roots.
 * Both directories must already exist and be distinct, non-nested absolute
 * directories. Symlinks in the supplied root names are canonicalized.
 * This operation is single-threaded, one-shot, and must precede PE loading.
 * It does not load game files, execute code, or create directories.
 * It does not sandbox later file access through the legacy host path mapper.
 */
int cricket07_engine_configure_game_paths(const char *game_directory,
                                          const char *user_directory);
int cricket07_engine_game_paths_configured(void);

#ifdef __cplusplus
}
#endif
#endif
