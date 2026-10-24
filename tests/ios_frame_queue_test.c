#include "FrameQueue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    char out[64] = {0};
    assert(cricket07_frame_queue_take(out, sizeof out) == 0);
    assert(cricket07_frame_queue_publish(NULL, 1) == 0);
    assert(cricket07_frame_queue_publish("alpha", 6) == 1);
    assert(cricket07_frame_queue_take(out, 2) == 0);
    assert(cricket07_frame_queue_publish("beta", 5) == 1);
    assert(cricket07_frame_queue_take(out, sizeof out) == 5);
    assert(strcmp(out, "beta") == 0);
    assert(cricket07_frame_queue_take(out, sizeof out) == 0);
    assert(cricket07_frame_queue_publish("reset", 6) == 1);
    cricket07_frame_queue_reset();
    assert(cricket07_frame_queue_take(out, sizeof out) == 0);
    puts("Frame queue tests passed");
    return 0;
}
