#ifndef CRICKET07_TEXTURE_UPLOAD_BRIDGE_H
#define CRICKET07_TEXTURE_UPLOAD_BRIDGE_H
#include <stddef.h>
#include <stdint.h>

/* Validated offsets into one CMD_UPLOAD_TEX byte buffer. No pixel pointers
 * escape the caller-owned buffer and no Metal objects are created here.
 * The consumer must copy/upload pixels before the input buffer is released.
 */
typedef struct {
    uint32_t texture_id;
    uint32_t levels;
    uint32_t width[14], height[14];
    size_t pixel_offset[14], pixel_bytes[14];
} Cricket07TextureUpload;

/* Returns 1 only for a complete, 8-byte-aligned RGBA8 mip chain (<=16 MiB).
 * Fails closed on malformed sizes, overflow, impossible mip dimensions and
 * excess trailing bytes. This is a staging decoder, not texture rendering.
 */
int cricket07_decode_texture_upload(const void *bytes, size_t length,
                                    Cricket07TextureUpload *out);
/* Generate a tiny synthetic RGBA8 mip chain for on-device Metal
 * allocation smoke testing. Never reads or distributes game assets. */
int cricket07_make_test_texture_upload(void *out, size_t capacity,
                                       size_t *written);
#endif
