#ifndef CRICKET07_FRAME_QUEUE_H
#define CRICKET07_FRAME_QUEUE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Isolated native command transport. This does not hook render_rec.c yet.
 * A frame is immutable once published. Reader receives its own copy. */
int cricket07_frame_queue_publish(const void *bytes, size_t length);
size_t cricket07_frame_queue_take(void *destination, size_t capacity);
void cricket07_frame_queue_reset(void);
#ifdef __cplusplus
}
#endif
#endif
