#include "RuntimeBridge.h"
#include "FrameQueue.h"
#include "render_cmd.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    cricket07_frame_queue_reset();
    float rgba[4] = {0};
    float xyzw[12] = {0};
    size_t count = 0;
    unsigned int frame = 0;

    /* Exercise a complete producer -> mailbox -> parser frame. */
    assert(cricket07_submit_test_triangle_frame() == 1);
    assert(cricket07_take_triangle_frame(rgba, 4, xyzw, 12, &count, &frame) == 1);
    assert(count == 3 && frame == 7);
    assert(fabsf(xyzw[0] + 0.7f) < 0.0001f);
    assert(fabsf(xyzw[4] - 0.7f) < 0.0001f);
    assert(fabsf(xyzw[9] - 0.7f) < 0.0001f);
    assert(xyzw[3] == 1.0f && xyzw[7] == 1.0f && xyzw[11] == 1.0f);
    assert(fabsf(rgba[0] - 22.0f / 255.0f) < 0.0001f);
    assert(fabsf(rgba[1] - 96.0f / 255.0f) < 0.0001f);
    assert(fabsf(rgba[2] - 120.0f / 255.0f) < 0.0001f);
    assert(rgba[3] == 1.0f);
    assert(cricket07_take_triangle_frame(rgba, 4, xyzw, 12, &count, &frame) == 0);

    /* A too-small destination must not overrun and must reject the draw. */
    assert(cricket07_submit_test_triangle_frame() == 1);
    assert(cricket07_take_triangle_frame(rgba, 4, xyzw, 11, &count, &frame) == 0);

    /* Malformed and unknown commands must fail closed. */
    CmdHdr bad = { CMD_DRAW, UINT32_MAX };
    assert(cricket07_frame_queue_publish(&bad, sizeof bad) == 1);
    assert(cricket07_take_triangle_frame(rgba, 4, xyzw, 12, &count, &frame) == 0);
    assert(cricket07_frame_queue_publish("bad", 3) == 1);
    assert(cricket07_take_triangle_frame(rgba, 4, xyzw, 12, &count, &frame) == 0);

    /* Null output and undersized color destination are rejected. */
    assert(cricket07_take_triangle_frame(NULL, 4, xyzw, 12, &count, &frame) == 0);
    assert(cricket07_take_triangle_frame(rgba, 3, xyzw, 12, &count, &frame) == 0);
    cricket07_frame_queue_reset();
    puts("iOS draw frame consumer regression passed");
    return 0;
}
