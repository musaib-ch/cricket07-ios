#include "RuntimeBridge.h"
#include "render_cmd.h"
#include <string.h>

/* First iOS adapter for the renderer's existing Direct3D command stream.
 * Safe bounded decoding of CMD_CLEAR; not yet a full command queue/executor.
 */
int cricket07_decode_clear_rgba(const void *bytes, size_t length, float *rgba, size_t count) {
    if (!bytes || !rgba || count < 4 || length < sizeof(CmdClear)) return 0;
    CmdClear command;
    memcpy(&command, bytes, sizeof(command));
    if (command.h.type != CMD_CLEAR || command.h.size < sizeof(CmdClear) ||
        command.h.size > length || (command.h.size & 7u) != 0 || !(command.flags & 1u)) return 0;
    rgba[0] = (float)((command.color >> 16) & 255u) / 255.0f;
    rgba[1] = (float)((command.color >> 8) & 255u) / 255.0f;
    rgba[2] = (float)(command.color & 255u) / 255.0f;
    rgba[3] = (float)((command.color >> 24) & 255u) / 255.0f;
    return 1;
}
int cricket07_sample_clear_rgba(float *rgba, size_t count) {
    CmdClear cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.h.type = CMD_CLEAR;
    cmd.h.size = (uint32_t)((sizeof(cmd) + 7u) & ~7u);
    cmd.flags = 1u;
    cmd.color = 0xFF166078u;
    return cricket07_decode_clear_rgba(&cmd, sizeof(cmd), rgba, count);
}
