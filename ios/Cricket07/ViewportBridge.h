#ifndef CRICKET07_VIEWPORT_BRIDGE_H
#define CRICKET07_VIEWPORT_BRIDGE_H
#include <stdint.h>
#include "render_cmd.h"

/* Safe CPU reference for the viewport math in host/render_wgpu.c.
 * Values are target pixel coordinates suitable for MTLViewport.
 * This helper does not apply the viewport to Metal or render game content.
 */
typedef struct {
    double x, y, width, height, min_depth, max_depth;
} Cricket07Viewport;

int cricket07_resolve_viewport(const CmdDraw *draw,
                              uint32_t target_width, uint32_t target_height,
                              uint32_t logical_width, uint32_t logical_height,
                              Cricket07Viewport *out);
#endif
