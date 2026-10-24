#ifndef CRICKET07_TRIANGLE_STAGING_H
#define CRICKET07_TRIANGLE_STAGING_H
#include <stddef.h>
int cricket07_extract_triangles(const void *data, size_t length,
                                float *xyzw, size_t float_capacity,
                                size_t *out_vertices);
#endif
