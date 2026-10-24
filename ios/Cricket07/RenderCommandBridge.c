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

/* Decode a complete bounded native command stream. This is the first
 * host-to-Metal transport boundary. Until the game producer is wired up,
 * only TARGET, CLEAR and PRESENT are accepted; reject anything else rather
 * than silently displaying an incomplete game frame.
 */
int cricket07_decode_frame_stream(const void *bytes, size_t length,
                                  float *rgba, size_t count,
                                  unsigned int *frame_number) {
    if (!bytes || !rgba || !frame_number || count < 4 || length == 0 ||
        length > 16u * 1024u * 1024u) return 0;
    size_t offset = 0;
    int cleared = 0, presented = 0;
    float color[4] = {0, 0, 0, 1};
    unsigned int frame = 0;
    while (offset < length) {
        if (length - offset < sizeof(CmdHdr)) return 0;
        CmdHdr header;
        memcpy(&header, (const unsigned char *)bytes + offset, sizeof(header));
        if (header.size < sizeof(CmdHdr) || (header.size & 7u) ||
            header.size > length - offset || presented) return 0;
        const unsigned char *current = (const unsigned char *)bytes + offset;
        switch (header.type) {
        case CMD_TARGET: {
            if (header.size < sizeof(CmdTarget)) return 0;
            CmdTarget target;
            memcpy(&target, current, sizeof(target));
            if (target.color_id != 0) return 0; /* backbuffer only */
            break;
        }
        case CMD_CLEAR:
            if (!cricket07_decode_clear_rgba(current, header.size, color, 4)) return 0;
            cleared = 1;
            break;
        case CMD_PRESENT: {
            if (header.size < sizeof(CmdPresent) || !cleared) return 0;
            CmdPresent present;
            memcpy(&present, current, sizeof(present));
            frame = present.frame;
            presented = 1;
            break;
        }
        default:
            return 0; /* draw/texture/shader support is not yet implemented */
        }
        offset += header.size;
    }
    if (!presented) return 0;
    memcpy(rgba, color, sizeof(color));
    *frame_number = frame;
    return 1;
}

int cricket07_sample_frame_stream(float *rgba, size_t count, unsigned int *frame_number) {
    struct { CmdClear clear; CmdPresent present; } stream;
    memset(&stream, 0, sizeof(stream));
    stream.clear.h.type = CMD_CLEAR;
    stream.clear.h.size = sizeof(CmdClear);
    stream.clear.flags = 1u;
    stream.clear.color = 0xFF166078u;
    stream.present.h.type = CMD_PRESENT;
    stream.present.h.size = sizeof(CmdPresent);
    stream.present.frame = 7;
    return cricket07_decode_frame_stream(&stream, sizeof(stream), rgba, count, frame_number);
}

/* Thread-safe latest-frame mailbox for native game-thread -> Metal main-thread
 * transfer. Only simple clear/present frames are accepted at this stage.
 * Never pass pointers into producer-owned memory to the consumer. */
#include <pthread.h>
static pthread_mutex_t g_frame_lock = PTHREAD_MUTEX_INITIALIZER;
static float g_latest_color[4];
static unsigned int g_latest_frame;
static unsigned long long g_frame_generation;
static int g_frame_available;

int cricket07_submit_frame(const void *bytes, size_t length) {
    float color[4];
    unsigned int frame;
    if (!cricket07_decode_frame_stream(bytes, length, color, 4, &frame)) return 0;
    pthread_mutex_lock(&g_frame_lock);
    memcpy(g_latest_color, color, sizeof(color));
    g_latest_frame = frame;
    g_frame_generation++;
    g_frame_available = 1;
    pthread_mutex_unlock(&g_frame_lock);
    return 1;
}

int cricket07_take_frame(float *rgba, size_t count, unsigned int *frame_number,
                         unsigned long long *generation) {
    if (!rgba || count < 4 || !frame_number || !generation) return 0;
    pthread_mutex_lock(&g_frame_lock);
    int new_frame = g_frame_available && g_frame_generation != *generation;
    if (new_frame) {
        memcpy(rgba, g_latest_color, sizeof(g_latest_color));
        *frame_number = g_latest_frame;
        *generation = g_frame_generation;
    }
    pthread_mutex_unlock(&g_frame_lock);
    return new_frame;
}

/* Deterministic producer test; does not imply the real game is linked. */
int cricket07_submit_test_frame(void) {
    struct { CmdClear clear; CmdPresent present; } sample;
    memset(&sample, 0, sizeof(sample));
    sample.clear.h.type = CMD_CLEAR;
    sample.clear.h.size = sizeof(CmdClear);
    sample.clear.flags = 1;
    sample.clear.color = 0xFF166078u;
    sample.present.h.type = CMD_PRESENT;
    sample.present.h.size = sizeof(CmdPresent);
    sample.present.frame = 7;
    return cricket07_submit_frame(&sample, sizeof(sample));
}
