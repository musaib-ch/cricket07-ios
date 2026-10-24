#ifndef CRICKET07_RUNTIME_BRIDGE_H
#define CRICKET07_RUNTIME_BRIDGE_H
#include <stddef.h>
/// Returns 1 when the linked C runtime function registry passes a deterministic smoke test.
/// Does not allocate the 4 GiB guest address space or execute Cricket 07.
int cricket07_runtime_linked(void);
int cricket07_runtime_registry_smoke_test(void);
/// Returns 1 if a 4 GiB + 64 KiB virtual reservation and two page touches succeed.
int cricket07_guest_memory_probe(void);
/// Disposable CPU, stack and thread-information-block layout test.
int cricket07_cpu_stack_probe(void);
/// Convert a bounded Direct3D clear command into normalized RGBA for iOS Metal.
int cricket07_decode_clear_rgba(const void *bytes, size_t length, float *rgba, size_t count);
/// Deterministic graphics command smoke test, not the live game renderer.
int cricket07_sample_clear_rgba(float *rgba, size_t count);
/// Decode an aligned TARGET/CLEAR/PRESENT command sequence; returns 0 for unsupported draw commands.
int cricket07_decode_frame_stream(const void *bytes, size_t length, float *rgba, size_t count, unsigned int *frame_number);
int cricket07_sample_frame_stream(float *rgba, size_t count, unsigned int *frame_number);
/// Submit a validated bounded frame from a producer thread.
int cricket07_submit_frame(const void *bytes, size_t length);
/// Read latest frame only when generation changes.
int cricket07_take_frame(float *rgba, size_t count, unsigned int *frame_number, unsigned long long *generation);
/// Submit one synthetic diagnostic frame, not actual gameplay.
int cricket07_submit_test_frame(void);
/// Synthetic Direct3D CMD_DRAW passed through bounded staging, no live game.
int cricket07_sample_draw_vertices(float *xyzw, size_t capacity, size_t *count);
/// Consume one strictly supported raw DRAW frame from the native queue, not a complete renderer.
int cricket07_take_triangle_frame(float *rgba, size_t color_capacity, float *xyzw, size_t vertex_float_capacity, size_t *vertex_count, unsigned int *frame_number);
/// Publish one synthetic CLEAR/DRAW/PRESENT frame through the actual native mailbox.
int cricket07_submit_test_triangle_frame(void);
#endif
