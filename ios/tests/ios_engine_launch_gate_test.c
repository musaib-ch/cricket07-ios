#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../engine/EngineLaunchGate.h"

static int loaded, linked, stage_ok, install_ok, resolve_ok, wrong_address;
static int stage_calls, install_calls, resolve_calls;

static void sentinel(CPU *cpu) { (void)cpu; }
static void translated(CPU *cpu) { (void)cpu; }

int cricket07_engine_image_loaded(void) { return loaded; }
int cricket07_engine_linked_translated_functions_available(void) { return linked; }
int cricket07_engine_stage_user_game(const char *game, const char *user,
                                    const char *exe, LoadedImage *out) {
    ++stage_calls;
    assert(strcmp(game, "/game") == 0);
    assert(strcmp(user, "/user") == 0);
    assert(strcmp(exe, "/game/cricket.exe") == 0);
    if (!stage_ok) return 0;
    loaded = 1;
    *out = (LoadedImage){0x400000u, 0x401000u, 2u};
    return 1;
}
int cricket07_engine_install_linked_translated_functions(void) {
    ++install_calls;
    return install_ok;
}
int cricket07_engine_resolve_loaded_entrypoint(GuestFn *fn, uint32_t *address) {
    ++resolve_calls;
    if (!resolve_ok) return 0;
    *fn = translated;
    *address = wrong_address ? 0x402000u : 0x401000u;
    return 1;
}

static void reset(void) {
    loaded = linked = stage_ok = install_ok = resolve_ok = wrong_address = 0;
    stage_calls = install_calls = resolve_calls = 0;
}

static Cricket07LaunchStatus run(LoadedImage *image, GuestFn *fn, uint32_t *addr) {
    return cricket07_engine_prepare_launch("/game", "/user", "/game/cricket.exe",
                                            image, fn, addr);
}

static void assert_untouched(const LoadedImage *image, GuestFn fn, uint32_t addr) {
    assert(image->image_base == 0xaaaaaaaau);
    assert(image->entry == 0xbbbbbbbbu);
    assert(image->num_imports == 0xccccccccu);
    assert(fn == sentinel);
    assert(addr == 0xddddddddu);
}

int main(void) {
    LoadedImage image;
    GuestFn fn;
    uint32_t addr;
#define RESET_OUTPUT() do { image = (LoadedImage){0xaaaaaaaau, 0xbbbbbbbbu, 0xccccccccu}; \
                           fn = sentinel; addr = 0xddddddddu; } while (0)

    reset();
    RESET_OUTPUT();
    assert(cricket07_engine_prepare_launch(0, "/user", "/game/cricket.exe",
                                            &image, &fn, &addr) ==
           CRICKET07_LAUNCH_INVALID_ARGUMENT);
    assert(cricket07_engine_prepare_launch("/game", "/user", "/game/cricket.exe",
                                            &image, 0, &addr) ==
           CRICKET07_LAUNCH_INVALID_ARGUMENT);
    assert(cricket07_engine_prepare_launch("game", "/user", "/game/cricket.exe",
                                            &image, &fn, &addr) ==
           CRICKET07_LAUNCH_INVALID_ARGUMENT);
    assert(stage_calls == 0);
    assert_untouched(&image, fn, addr);

    reset();
    RESET_OUTPUT();
    assert(run(&image, &fn, &addr) == CRICKET07_LAUNCH_REGISTRY_UNAVAILABLE);
    assert(stage_calls == 0 && install_calls == 0);
    assert_untouched(&image, fn, addr);

    reset();
    RESET_OUTPUT();
    linked = 1;
    assert(run(&image, &fn, &addr) == CRICKET07_LAUNCH_STAGE_FAILED);
    assert(stage_calls == 1 && install_calls == 0);
    assert_untouched(&image, fn, addr);

    stage_ok = 1;
    assert(run(&image, &fn, &addr) == CRICKET07_LAUNCH_REGISTRATION_FAILED);
    assert(stage_calls == 2 && install_calls == 1 && resolve_calls == 0);
    assert_untouched(&image, fn, addr);
    assert(run(&image, &fn, &addr) == CRICKET07_LAUNCH_ALREADY_STAGED);
    assert(stage_calls == 2 && install_calls == 1);

    reset();
    RESET_OUTPUT();
    linked = stage_ok = install_ok = 1;
    assert(run(&image, &fn, &addr) == CRICKET07_LAUNCH_ENTRY_UNRESOLVED);
    assert(resolve_calls == 1);
    assert_untouched(&image, fn, addr);

    reset();
    RESET_OUTPUT();
    linked = stage_ok = install_ok = resolve_ok = wrong_address = 1;
    assert(run(&image, &fn, &addr) == CRICKET07_LAUNCH_ENTRY_UNRESOLVED);
    assert_untouched(&image, fn, addr);

    reset();
    RESET_OUTPUT();
    linked = stage_ok = install_ok = resolve_ok = 1;
    assert(run(&image, &fn, &addr) == CRICKET07_LAUNCH_READY);
    assert(image.image_base == 0x400000u && image.entry == 0x401000u);
    assert(image.num_imports == 2u);
    assert(fn == translated && addr == image.entry);
    assert(stage_calls == 1 && install_calls == 1 && resolve_calls == 1);
    assert(run(&image, &fn, &addr) == CRICKET07_LAUNCH_ALREADY_STAGED);
    return 0;
}
