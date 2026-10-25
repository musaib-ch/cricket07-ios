#include "ControllerInputQueue.h"
#include <pthread.h>

/* HID usages match host/input.c: arrows 80/79/82/81, Enter 40, Escape 41. */
static const struct { uint16_t vk, hid; } keys[] = {
    {0x25u, 80u}, {0x27u, 79u}, {0x26u, 82u},
    {0x28u, 81u}, {0x0du, 40u}, {0x1bu, 41u}
};
#define KEY_COUNT (sizeof(keys) / sizeof(keys[0]))
#define QUEUE_CAPACITY 128u
static Cricket07HIDEvent queue[QUEUE_CAPACITY];
static unsigned char held[KEY_COUNT];
static size_t head, count;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void append(uint16_t usage, int pressed) {
    size_t index = (head + count) % QUEUE_CAPACITY;
    queue[index] = (Cricket07HIDEvent){ usage, (uint8_t)(pressed != 0), 0 };
    ++count;
}

static void release_all(void) {
    head = 0;
    count = 0;
    for (size_t i = 0; i < KEY_COUNT; ++i) {
        if (held[i]) {
            append(keys[i].hid, 0);
            held[i] = 0;
        }
    }
}

int cricket07_controller_enqueue_vk(uint16_t virtual_key, int pressed) {
    size_t i;
    for (i = 0; i < KEY_COUNT; ++i)
        if (keys[i].vk == virtual_key) break;
    if (i == KEY_COUNT) return 0;
    pressed = pressed != 0;
    pthread_mutex_lock(&lock);
    if (held[i] == (unsigned char)pressed) {
        pthread_mutex_unlock(&lock);
        return 0;
    }
    if (count == QUEUE_CAPACITY) {
        /* Preserve safety over transient input: releases replace stale events.
         * Never enqueue the event that caused overflow. */
        release_all();
        pthread_mutex_unlock(&lock);
        return 0;
    }
    held[i] = (unsigned char)pressed;
    append(keys[i].hid, pressed);
    pthread_mutex_unlock(&lock);
    return 1;
}

size_t cricket07_controller_drain(Cricket07HIDEvent *events, size_t capacity) {
    if (!events || !capacity) return 0;
    pthread_mutex_lock(&lock);
    size_t n = count < capacity ? count : capacity;
    for (size_t i = 0; i < n; ++i)
        events[i] = queue[(head + i) % QUEUE_CAPACITY];
    head = (head + n) % QUEUE_CAPACITY;
    count -= n;
    pthread_mutex_unlock(&lock);
    return n;
}

void cricket07_controller_reset(void) {
    pthread_mutex_lock(&lock);
    release_all();
    pthread_mutex_unlock(&lock);
}
