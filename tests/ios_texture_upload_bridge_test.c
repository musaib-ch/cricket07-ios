#include "TextureUploadBridge.h"
#include "render_cmd.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    unsigned char bytes[256] = {0};
    CmdUploadTex command = {0};
    command.h.type = CMD_UPLOAD_TEX;
    command.h.size = (uint32_t)((sizeof(command) + 16 + 4 + 7) & ~(size_t)7);
    command.id = 42;
    command.levels = 2;
    command.w[0] = 2; command.h_[0] = 2;
    command.w[1] = 1; command.h_[1] = 1;
    memcpy(bytes, &command, sizeof(command));
    bytes[sizeof(command)] = 0xA5;
    bytes[sizeof(command) + 16] = 0x5A;
    Cricket07TextureUpload out = {0};
    assert(cricket07_decode_texture_upload(bytes, command.h.size, &out) == 1);
    assert(out.texture_id == 42 && out.levels == 2);
    assert(out.width[0] == 2 && out.height[1] == 1);
    assert(out.pixel_offset[0] == sizeof(command));
    assert(out.pixel_bytes[0] == 16 && out.pixel_bytes[1] == 4);
    assert(bytes[out.pixel_offset[0]] == 0xA5);
    assert(bytes[out.pixel_offset[1]] == 0x5A);

    assert(cricket07_decode_texture_upload(NULL, command.h.size, &out) == 0);
    assert(cricket07_decode_texture_upload(bytes, command.h.size, NULL) == 0);
    assert(cricket07_decode_texture_upload(bytes, sizeof(command), &out) == 0);
    assert(cricket07_decode_texture_upload(bytes, command.h.size - 1, &out) == 0);
    assert(cricket07_decode_texture_upload(bytes, command.h.size + 8, &out) == 0);

    CmdUploadTex bad = command;
    bad.levels = 15;
    memcpy(bytes, &bad, sizeof(bad));
    assert(cricket07_decode_texture_upload(bytes, command.h.size, &out) == 0);
    bad = command; bad.w[0] = UINT32_MAX;
    memcpy(bytes, &bad, sizeof(bad));
    assert(cricket07_decode_texture_upload(bytes, command.h.size, &out) == 0);
    bad = command; bad.h_[1] = 2; /* invalid mip progression */
    memcpy(bytes, &bad, sizeof(bad));
    assert(cricket07_decode_texture_upload(bytes, command.h.size, &out) == 0);
    bad = command; bad.id = 0;
    memcpy(bytes, &bad, sizeof(bad));
    assert(cricket07_decode_texture_upload(bytes, command.h.size, &out) == 0);
    bad = command; bad.h.type = CMD_DRAW;
    memcpy(bytes, &bad, sizeof(bad));
    assert(cricket07_decode_texture_upload(bytes, command.h.size, &out) == 0);
    bad = command; bad.h.size = sizeof(command);
    memcpy(bytes, &bad, sizeof(bad));
    assert(cricket07_decode_texture_upload(bytes, command.h.size, &out) == 0);

    /* A valid single 1x1 mip occupies 4 bytes followed by 4 bytes padding. */
    bad = command; bad.levels = 1; bad.w[0] = bad.h_[0] = 1;
    bad.h.size = (uint32_t)((sizeof(bad) + 4 + 7) & ~(size_t)7);
    memcpy(bytes, &bad, sizeof(bad));
    assert(cricket07_decode_texture_upload(bytes, bad.h.size, &out) == 1);
    assert(out.levels == 1 && out.pixel_bytes[0] == 4);
    /* The Metal allocation smoke test must be a complete, decodable
     * synthetic command and must never write beyond caller capacity. */
    unsigned char sample[256] = {0};
    size_t written = 0;
    assert(cricket07_make_test_texture_upload(sample, sizeof(sample), &written) == 1);
    assert(written <= sizeof(sample));
    assert(cricket07_decode_texture_upload(sample, written, &out) == 1);
    assert(out.texture_id == 1 && out.levels == 2);
    assert(out.width[0] == 2 && out.height[0] == 2);
    assert(out.width[1] == 1 && out.height[1] == 1);
    assert(sample[out.pixel_offset[0]] == 255);
    assert(sample[out.pixel_offset[1]] == 128);
    assert(cricket07_make_test_texture_upload(sample, written - 1, &written) == 0);
    assert(cricket07_make_test_texture_upload(NULL, sizeof(sample), &written) == 0);
    assert(cricket07_make_test_texture_upload(sample, sizeof(sample), NULL) == 0);
    puts("iOS texture upload decoder regression passed");
    return 0;
}
