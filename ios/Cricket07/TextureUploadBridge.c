#include "TextureUploadBridge.h"
#include "render_cmd.h"
#include <string.h>

enum { CRICKET07_MAX_TEX_DIM = 8192, CRICKET07_MAX_TEX_COMMAND = 16 * 1024 * 1024 };

int cricket07_decode_texture_upload(const void *bytes, size_t length,
                                    Cricket07TextureUpload *out) {
    if (!bytes || !out || length < sizeof(CmdUploadTex) ||
        length > CRICKET07_MAX_TEX_COMMAND || (length & 7u)) return 0;
    CmdUploadTex command;
    memcpy(&command, bytes, sizeof(command)); /* input may be unaligned */
    if (command.h.type != CMD_UPLOAD_TEX || command.h.size != length ||
        !command.id || command.levels == 0 || command.levels > 14) return 0;

    Cricket07TextureUpload result = {0};
    result.texture_id = command.id;
    result.levels = command.levels;
    size_t offset = sizeof(command);
    for (uint32_t i = 0; i < command.levels; ++i) {
        uint32_t w = command.w[i], h = command.h_[i];
        if (!w || !h || w > CRICKET07_MAX_TEX_DIM || h > CRICKET07_MAX_TEX_DIM) return 0;
        if (i && (w != (result.width[i-1] > 1 ? result.width[i-1] / 2 : 1) ||
                  h != (result.height[i-1] > 1 ? result.height[i-1] / 2 : 1))) return 0;
        /* Division-based bound check prevents multiplication/offset overflow. */
        if ((size_t)w > (length - offset) / 4u / (size_t)h) return 0;
        size_t size = (size_t)w * (size_t)h * 4u;
        result.width[i] = w;
        result.height[i] = h;
        result.pixel_offset[i] = offset;
        result.pixel_bytes[i] = size;
        offset += size;
    }
    /* The recorder aligns each command to 8 bytes. Allow only alignment
     * padding, never a second hidden payload or an additional command. */
    if (length - offset > 7u || ((offset + 7u) & ~(size_t)7u) != length) return 0;
    *out = result;
    return 1;
}

/* A deterministic 2x2 RGBA8 texture and its 1x1 mip. Diagnostic only. */
int cricket07_make_test_texture_upload(void *out, size_t capacity,
                                       size_t *written) {
    const size_t payload = 20u;
    const size_t length = (sizeof(CmdUploadTex) + payload + 7u) & ~(size_t)7u;
    if (!out || !written || capacity < length) return 0;
    CmdUploadTex cmd = {0};
    cmd.h.type = CMD_UPLOAD_TEX;
    cmd.h.size = (uint32_t)length;
    cmd.id = 1u;
    cmd.levels = 2u;
    cmd.w[0] = cmd.h_[0] = 2u;
    cmd.w[1] = cmd.h_[1] = 1u;
    static const unsigned char pixels[20] = {
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255,
        128, 128, 128, 255
    };
    memset(out, 0, length);
    memcpy(out, &cmd, sizeof(cmd));
    memcpy((unsigned char *)out + sizeof(cmd), pixels, sizeof(pixels));
    *written = length;
    return 1;
}
