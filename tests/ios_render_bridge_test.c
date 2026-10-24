#include "RuntimeBridge.h"
#include "render_cmd.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    float rgba[4] = {0};
    unsigned int frame = 0;
    assert(cricket07_sample_frame_stream(rgba, 4, &frame) == 1);
    assert(frame == 7);
    assert(cricket07_sample_frame_stream(NULL, 4, &frame) == 0);
    assert(cricket07_sample_frame_stream(rgba, 3, &frame) == 0);
    assert(cricket07_sample_frame_stream(rgba, 4, NULL) == 0);
    struct { CmdClear clear; CmdPresent present; } commands;
    memset(&commands, 0, sizeof(commands));
    commands.clear.h.type = CMD_CLEAR;
    commands.clear.h.size = sizeof(CmdClear);
    commands.clear.flags = 1;
    commands.clear.color = 0xff123456;
    commands.present.h.type = CMD_PRESENT;
    commands.present.h.size = sizeof(CmdPresent);
    commands.present.frame = 22;
    assert(cricket07_decode_frame_stream(&commands, sizeof(commands), rgba, 4, &frame) == 1);
    assert(frame == 22);
    assert(cricket07_decode_frame_stream(&commands, sizeof(commands) - 1, rgba, 4, &frame) == 0);
    commands.clear.h.size = UINT32_MAX;
    assert(cricket07_decode_frame_stream(&commands, sizeof(commands), rgba, 4, &frame) == 0);
    commands.clear.h.size = sizeof(CmdClear);
    commands.clear.h.type = CMD_DRAW;
    assert(cricket07_decode_frame_stream(&commands, sizeof(commands), rgba, 4, &frame) == 0);
    puts("Graphics bridge tests passed");
    return 0;
}
