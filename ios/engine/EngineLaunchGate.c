#include "EngineLaunchGate.h"
#include "EngineBootstrap.h"
#include "EngineStartup.h"

Cricket07LaunchStatus cricket07_engine_prepare_launch(
    const char *game_directory, const char *user_directory,
    const char *executable_path, LoadedImage *out_image,
    GuestFn *out_entry, uint32_t *out_entry_address) {
    if (!game_directory || !user_directory || !executable_path ||
        game_directory[0] != '/' || user_directory[0] != '/' ||
        executable_path[0] != '/' || !out_image || !out_entry ||
        !out_entry_address)
        return CRICKET07_LAUNCH_INVALID_ARGUMENT;

    if (cricket07_engine_image_loaded())
        return CRICKET07_LAUNCH_ALREADY_STAGED;

    /* Do not consume the one-shot PE loader in diagnostic builds. */
    if (!cricket07_engine_linked_translated_functions_available())
        return CRICKET07_LAUNCH_REGISTRY_UNAVAILABLE;

    LoadedImage staged = {0};
    if (!cricket07_engine_stage_user_game(game_directory, user_directory,
                                          executable_path, &staged))
        return CRICKET07_LAUNCH_STAGE_FAILED;

    /* Do not run the one-shot registrar unless the loader actually
     * published a coherent image. A faulty staging adapter must not be
     * allowed to claim success without publishing guest memory. */
    if (!cricket07_engine_image_loaded() || staged.image_base == 0 ||
        staged.entry < staged.image_base)
        return CRICKET07_LAUNCH_STAGE_FAILED;

    /* A failed registrar may have mutated the runtime function table.
     * The staged image is intentionally not rolled back or retried. */
    if (!cricket07_engine_install_linked_translated_functions())
        return CRICKET07_LAUNCH_REGISTRATION_FAILED;

    GuestFn entry = 0;
    uint32_t address = 0;
    if (!cricket07_engine_resolve_loaded_entrypoint(&entry, &address) ||
        !entry || address != staged.entry)
        return CRICKET07_LAUNCH_ENTRY_UNRESOLVED;

    *out_image = staged;
    *out_entry = entry;
    *out_entry_address = address;
    return CRICKET07_LAUNCH_READY;
}
