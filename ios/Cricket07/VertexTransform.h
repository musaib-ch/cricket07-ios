#ifndef CRICKET07_VERTEX_TRANSFORM_H
#define CRICKET07_VERTEX_TRANSFORM_H

#include <stddef.h>
#include "render_cmd.h"

/* CPU reference for the position math in host/render_wgpu.c's WGSL vs().
 * Converts positions from a single CMD_DRAW to Metal clip coordinates.
 * This does not implement the actual GPU viewport, texturing or render states.
 * Returns 0 for invalid input without committing a partially transformed output.
 */
int cricket07_transform_draw_positions(const CmdDraw *draw,
                                       const float *input_xyzw,
                                       size_t vertex_count,
                                       float *output_xyzw,
                                       size_t output_float_capacity);
#endif
