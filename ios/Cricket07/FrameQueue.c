#include "FrameQueue.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#define MAX_FRAME_BYTES (16u * 1024u * 1024u)
static pthread_mutex_t frame_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned char *pending;
static size_t pending_length;
int cricket07_frame_queue_publish(const void *bytes, size_t length) {
    if (!bytes || length == 0 || length > MAX_FRAME_BYTES) return 0;
    unsigned char *copy = malloc(length);
    if (!copy) return 0;
    memcpy(copy, bytes, length);
    pthread_mutex_lock(&frame_lock);
    free(pending);
    pending = copy;
    pending_length = length;
    pthread_mutex_unlock(&frame_lock);
    return 1;
}
size_t cricket07_frame_queue_take(void *destination, size_t capacity) {
    if (!destination) return 0;
    pthread_mutex_lock(&frame_lock);
    size_t length = pending_length;
    if (!pending || capacity < length) {
        pthread_mutex_unlock(&frame_lock);
        return 0;
    }
    memcpy(destination, pending, length);
    free(pending);
    pending = NULL;
    pending_length = 0;
    pthread_mutex_unlock(&frame_lock);
    return length;
}
void cricket07_frame_queue_reset(void) {
    pthread_mutex_lock(&frame_lock);
    free(pending);
    pending = NULL;
    pending_length = 0;
    pthread_mutex_unlock(&frame_lock);
}
