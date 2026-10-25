#include "EngineRenderBackend.h"
#include "../../host/render.h"

/* The translated desktop launcher normally selects g_render_gpu after
 * creating its SDL window. iOS has no SDL window, so a future Swift-driven
 * engine bootstrap must select the recorder explicitly before D3D startup.
 * Do not silently select the headless backend or execute the game here.
 */
int cricket07_engine_select_ios_recorder(void) {
#if defined(CRICKET07_IOS_GAME) && defined(RC_IOS_FRAME_TRANSPORT)
    if (!g_render_gpu.init || !g_render_gpu.present) return 0;
    g_render = &g_render_gpu;
    return 1;
#else
    return 0;
#endif
}

int cricket07_engine_ios_recorder_selected(void) {
#if defined(CRICKET07_IOS_GAME) && defined(RC_IOS_FRAME_TRANSPORT)
    return g_render == &g_render_gpu;
#else
    return 0;
#endif
}
