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

/* A draw range and its own viewport. A flattened vertex buffer alone cannot
 * represent different viewport states for consecutive CMD_DRAW commands. */
/* Normalized Direct3D depth and cull state for a single Metal draw.
 * depth_compare uses D3DCMP values 1..8; cull_mode is 0:none, 1:front, 2:back.
 */
typedef struct {
    uint32_t depth_compare, depth_write, cull_mode;
    /* PKey blend and color mask, normalized for per-draw Metal pipelines. */
    uint32_t blend, src_blend, dst_blend, blend_op, color_write_mask;
} Cricket07DrawState;

typedef struct {
    Cricket07Viewport viewport;
    uint32_t first_vertex, vertex_count;
    Cricket07DrawState state;
} Cricket07DrawCall;

/* Reject unsupported state values rather than guessing Metal behavior. */
int cricket07_resolve_draw_state(const CmdDraw *draw, Cricket07DrawState *out);

int cricket07_resolve_viewport(const CmdDraw *draw,
                              uint32_t target_width, uint32_t target_height,
                              uint32_t logical_width, uint32_t logical_height,
                              Cricket07Viewport *out);
#endif
