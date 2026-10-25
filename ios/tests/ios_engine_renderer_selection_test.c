#include <assert.h>
#include <stdio.h>
#include "../engine/EngineRenderBackend.h"
#include "../../host/render.h"

/* Synthetic backends: no game assets, Metal device, or translated game code. */
static int fake_init(Device *device) { (void)device; return 1; }
static void fake_present(Device *device) { (void)device; }
RenderBackend g_render_headless = { .name = "headless" };
RenderBackend g_render_gpu = { .name = "recorder" };
RenderBackend *g_render = &g_render_headless;

int main(void) {
    assert(cricket07_engine_ios_recorder_selected() == 0);
#if defined(CRICKET07_IOS_GAME) && defined(RC_IOS_FRAME_TRANSPORT)
    /* An incomplete backend must not replace the safe default. */
    assert(cricket07_engine_select_ios_recorder() == 0);
    assert(g_render == &g_render_headless);
    g_render_gpu.init = fake_init;
    assert(cricket07_engine_select_ios_recorder() == 0);
    assert(g_render == &g_render_headless);
    g_render_gpu.present = fake_present;
    assert(cricket07_engine_select_ios_recorder() == 1);
    assert(g_render == &g_render_gpu);
    assert(cricket07_engine_ios_recorder_selected() == 1);
    assert(cricket07_engine_select_ios_recorder() == 1); /* idempotent */
#else
    g_render_gpu.init = fake_init;
    g_render_gpu.present = fake_present;
    assert(cricket07_engine_select_ios_recorder() == 0);
    assert(g_render == &g_render_headless);
    assert(cricket07_engine_ios_recorder_selected() == 0);
#endif
    puts("Engine renderer selection smoke tests passed");
    return 0;
}
