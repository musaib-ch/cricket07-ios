#include "EngineStartup.h"
#include "EngineBootstrap.h"
#include "EngineGamePaths.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* A pre-execution composition point for the future dedicated engine thread.
 * This is a path gate, not an OS sandbox or a defense against concurrent
 * filesystem mutation. No generated game code is invoked here.
 */
int cricket07_engine_stage_user_game(const char *game_directory,
                                    const char *user_directory,
                                    const char *executable_path,
                                    LoadedImage *out) {
    if (!game_directory || !user_directory || !executable_path || !out ||
        game_directory[0] != '/' || user_directory[0] != '/' ||
        executable_path[0] != '/' || cricket07_engine_image_loaded())
        return 0;

    char *game = realpath(game_directory, NULL);
    char *executable = realpath(executable_path, NULL);
    if (!game || !executable) {
        free(game);
        free(executable);
        return 0;
    }

    struct stat game_stat, exe_stat;
    size_t game_len = strlen(game);
    size_t exe_len = strlen(executable);
    /* The legacy host stores roots in 1024-byte arrays. Keep paths within
     * those bounds and require the executable to remain below the game root.
     * A sibling prefix such as /game-other must not match /game.
     */
    int valid = game_len > 1 && game_len < 1024 &&
                exe_len > game_len + 1 && exe_len < 1024 &&
                strncmp(game, executable, game_len) == 0 &&
                executable[game_len] == '/' &&
                stat(game, &game_stat) == 0 && S_ISDIR(game_stat.st_mode) &&
                stat(executable, &exe_stat) == 0 && S_ISREG(exe_stat.st_mode) &&
                access(executable, R_OK) == 0;

    if (valid)
        valid = cricket07_engine_configure_game_paths(game, user_directory);
    if (valid)
        valid = cricket07_engine_load_user_executable(executable, out);

    free(game);
    free(executable);
    return valid;
}
