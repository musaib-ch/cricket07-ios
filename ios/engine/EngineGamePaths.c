#include "EngineGamePaths.h"
#include "../../runtime/runtime.h"

/* Defined by host/core.c, with the same 1024-byte layout as win32.h. */
extern char g_game_root[1024];
extern char g_user_root[1024];

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* These globals are owned by host/core.c. This adapter is called only on
 * the future dedicated game startup thread, before any host file access. */
static int g_paths_configured;

static int canonical_directory(const char *input, char output[1024], int writable) {
    if (!input || input[0] != '/' || !input[1]) return 0;
    char *canonical = realpath(input, NULL);
    if (!canonical) return 0;
    struct stat st;
    size_t length = strlen(canonical);
    int ok = length > 1 && length < 1024 &&
             stat(canonical, &st) == 0 && S_ISDIR(st.st_mode) &&
             (!writable || access(canonical, W_OK | X_OK) == 0);
    if (ok) memcpy(output, canonical, length + 1);
    free(canonical);
    return ok;
}

static int same_or_nested(const char *parent, const char *child) {
    size_t length = strlen(parent);
    return strncmp(parent, child, length) == 0 &&
           (child[length] == '/' || child[length] == '\0');
}

int cricket07_engine_configure_game_paths(const char *game_directory,
                                          const char *user_directory) {
    /* No late reconfiguration after the guest image is published. */
    if (g_image_hi > g_image_lo) return 0;
    char game[1024], user[1024];
    if (!canonical_directory(game_directory, game, 0) ||
        !canonical_directory(user_directory, user, 1) ||
        same_or_nested(game, user) || same_or_nested(user, game)) return 0;
    if (g_paths_configured)
        return strcmp(g_game_root, game) == 0 &&
               strcmp(g_user_root, user) == 0;
    /* Never overwrite roots already configured by another launcher. */
    if (g_game_root[0] || g_user_root[0]) return 0;
    memcpy(g_game_root, game, strlen(game) + 1);
    memcpy(g_user_root, user, strlen(user) + 1);
    g_paths_configured = 1;
    return 1;
}

int cricket07_engine_game_paths_configured(void) {
    return g_paths_configured;
}
