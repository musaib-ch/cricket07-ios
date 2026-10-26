#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../engine/EngineStartup.h"

static int configure_calls, load_calls, loaded, loader_ok;
static char configured_game[PATH_MAX], configured_user[PATH_MAX];
static char loaded_path[PATH_MAX];

int cricket07_engine_image_loaded(void) { return loaded; }
int cricket07_engine_configure_game_paths(const char *game, const char *user) {
    ++configure_calls;
    if (!configured_game[0]) {
        snprintf(configured_game, sizeof configured_game, "%s", game);
        snprintf(configured_user, sizeof configured_user, "%s", user);
    }
    return strcmp(configured_game, game) == 0 &&
           strcmp(configured_user, user) == 0;
}
int cricket07_engine_load_user_executable(const char *path, LoadedImage *out) {
    ++load_calls;
    snprintf(loaded_path, sizeof loaded_path, "%s", path);
    if (!loader_ok) return 0;
    out->image_base = 0x400000u;
    out->entry = 0x401000u;
    loaded = 1;
    return 1;
}

static void touch(const char *path) {
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fputc('X', f) != EOF);
    assert(fclose(f) == 0);
}
int main(void) {
    char temp[] = "/tmp/cricket07-stage-XXXXXX";
    char *root = mkdtemp(temp);
    assert(root);
    char game[PATH_MAX], user[PATH_MAX], other[PATH_MAX];
    char exe[PATH_MAX], outside[PATH_MAX], escape[PATH_MAX], alias[PATH_MAX];
    char sibling[PATH_MAX], sibling_exe[PATH_MAX];
#define PATH_INTO(dst, suffix) assert(snprintf(dst, sizeof dst, "%s/" suffix, root) > 0)
    PATH_INTO(game, "game");
    PATH_INTO(user, "user");
    PATH_INTO(other, "other");
    PATH_INTO(exe, "game/cricket.exe");
    PATH_INTO(outside, "other/cricket.exe");
    PATH_INTO(escape, "game/escape.exe");
    PATH_INTO(alias, "alias");
    PATH_INTO(sibling, "game-other");
    PATH_INTO(sibling_exe, "game-other/cricket.exe");
    assert(mkdir(game, 0700) == 0);
    assert(mkdir(user, 0700) == 0);
    assert(mkdir(other, 0700) == 0);
    assert(mkdir(sibling, 0700) == 0);
    touch(exe);
    touch(outside);
    touch(sibling_exe);
    assert(symlink(outside, escape) == 0);
    assert(symlink(game, alias) == 0);
    char *canonical_game = realpath(game, NULL);
    char *canonical_exe = realpath(exe, NULL);
    assert(canonical_game && canonical_exe);

    LoadedImage image = {0xaaaaaaaau, 0xbbbbbbbbu, 0xccccccccu};
    assert(!cricket07_engine_stage_user_game(NULL, user, exe, &image));
    assert(!cricket07_engine_stage_user_game(game, user, exe, NULL));
    assert(!cricket07_engine_stage_user_game("relative", user, exe, &image));
    assert(!cricket07_engine_stage_user_game(game, user, outside, &image));
    assert(!cricket07_engine_stage_user_game(game, user, escape, &image));
    assert(!cricket07_engine_stage_user_game(game, user, sibling_exe, &image));
    assert(!cricket07_engine_stage_user_game(game, user, game, &image));
    assert(!cricket07_engine_stage_user_game(game, user, "/does/not/exist", &image));
    assert(configure_calls == 0 && load_calls == 0);
    assert(image.image_base == 0xaaaaaaaau && image.entry == 0xbbbbbbbbu);

    /* Configuration survives a failed PE load; same roots can retry. */
    assert(!cricket07_engine_stage_user_game(alias, user, exe, &image));
    assert(configure_calls == 1 && load_calls == 1);
    assert(strcmp(configured_game, canonical_game) == 0);
    assert(strcmp(loaded_path, canonical_exe) == 0);
    assert(image.image_base == 0xaaaaaaaau && image.entry == 0xbbbbbbbbu);
    loader_ok = 1;
    assert(cricket07_engine_stage_user_game(game, user, exe, &image));
    assert(configure_calls == 2 && load_calls == 2);
    assert(image.image_base == 0x400000u && image.entry == 0x401000u);
    assert(!cricket07_engine_stage_user_game(game, user, exe, &image));
    assert(configure_calls == 2 && load_calls == 2);

    free(canonical_game);
    free(canonical_exe);
    assert(unlink(alias) == 0 && unlink(escape) == 0);
    assert(unlink(exe) == 0 && unlink(outside) == 0);
    assert(unlink(sibling_exe) == 0);
    assert(rmdir(game) == 0 && rmdir(user) == 0);
    assert(rmdir(other) == 0 && rmdir(sibling) == 0);
    assert(rmdir(root) == 0);
    return 0;
}
