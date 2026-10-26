#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../engine/EngineGamePaths.h"

/* Isolated host globals, no proprietary game files or real guest mapping. */
char g_game_root[1024], g_user_root[1024];
unsigned int g_image_lo, g_image_hi;

int main(void) {
    char temp[] = "/tmp/cricket07-paths-XXXXXX";
    char *root = mkdtemp(temp);
    assert(root);
    char game[PATH_MAX], user[PATH_MAX], nested[PATH_MAX];
    char alias[PATH_MAX], missing[PATH_MAX], file[PATH_MAX];
    assert(snprintf(game, sizeof game, "%s/game", root) > 0);
    assert(snprintf(user, sizeof user, "%s/userdata", root) > 0);
    assert(snprintf(nested, sizeof nested, "%s/game/nested", root) > 0);
    assert(snprintf(alias, sizeof alias, "%s/game-link", root) > 0);
    assert(snprintf(missing, sizeof missing, "%s/missing", root) > 0);
    assert(snprintf(file, sizeof file, "%s/not-a-directory", root) > 0);
    assert(mkdir(game, 0700) == 0 && mkdir(user, 0700) == 0);
    assert(mkdir(nested, 0700) == 0);
    FILE *f = fopen(file, "wb");
    assert(f && fclose(f) == 0);
    assert(symlink(game, alias) == 0);
    /* On macOS /tmp is a symlink to /private/tmp. Compare canonical roots. */
    char *canonical_game = realpath(game, NULL);
    char *canonical_user = realpath(user, NULL);
    assert(canonical_game && canonical_user);

    assert(!cricket07_engine_game_paths_configured());
    assert(!cricket07_engine_configure_game_paths(NULL, user));
    assert(!cricket07_engine_configure_game_paths("relative", user));
    assert(!cricket07_engine_configure_game_paths(game, missing));
    assert(!cricket07_engine_configure_game_paths(game, file));
    assert(!cricket07_engine_configure_game_paths(game, game));
    assert(!cricket07_engine_configure_game_paths(game, nested));
    assert(!cricket07_engine_configure_game_paths(nested, game));
    assert(!g_game_root[0] && !g_user_root[0]);

    /* Canonicalization permits symlinked root aliases, without retaining
     * a mutable symlink spelling in the host's filesystem globals. */
    assert(cricket07_engine_configure_game_paths(alias, user));
    assert(cricket07_engine_game_paths_configured());
    assert(strcmp(g_game_root, canonical_game) == 0);
    assert(strcmp(g_user_root, canonical_user) == 0);
    assert(cricket07_engine_configure_game_paths(game, user));
    assert(!cricket07_engine_configure_game_paths(user, game));
    assert(!cricket07_engine_configure_game_paths(game, missing));
    assert(strcmp(g_game_root, canonical_game) == 0);
    assert(strcmp(g_user_root, canonical_user) == 0);

    g_image_lo = 0x400000u;
    g_image_hi = 0x404000u;
    assert(!cricket07_engine_configure_game_paths(game, user));
    assert(strcmp(g_game_root, canonical_game) == 0);
    assert(strcmp(g_user_root, canonical_user) == 0);
    assert(unlink(alias) == 0 && unlink(file) == 0);
    assert(rmdir(nested) == 0 && rmdir(game) == 0);
    assert(rmdir(user) == 0 && rmdir(root) == 0);
    free(canonical_game);
    free(canonical_user);
    return 0;
}
