#include "TriangleStaging.h"
#include "render_cmd.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
int main(void) {
    unsigned char bytes[sizeof(CmdDraw) + 3 * sizeof(CVert) + 8] = {0};
    CmdDraw draw = {0};
    draw.h.type = CMD_DRAW;
    draw.h.size = (uint32_t)((sizeof(CmdDraw) + 3 * sizeof(CVert) + 7u) & ~7u);
    draw.key.topo = 4;
    draw.nverts = 3;
    CVert vertices[3] = {0};
    vertices[0].x = -1.0f;
    vertices[1].x = 1.0f;
    vertices[2].y = 1.0f;
    memcpy(bytes, &draw, sizeof draw);
    memcpy(bytes + sizeof draw, vertices, sizeof vertices);
    float coordinates[12] = {0};
    size_t count = 0;
    assert(cricket07_extract_triangles(bytes, draw.h.size, coordinates, 12, &count));
    assert(count == 3 && coordinates[0] == -1.0f && coordinates[4] == 1.0f && coordinates[9] == 1.0f);
    assert(!cricket07_extract_triangles(bytes, sizeof(CmdDraw) - 1, coordinates, 12, &count));
    assert(!cricket07_extract_triangles(bytes, draw.h.size, coordinates, 11, &count));
    ((CmdDraw *)bytes)->key.topo = 2;
    assert(!cricket07_extract_triangles(bytes, draw.h.size, coordinates, 12, &count));
    puts("Triangle staging tests passed");
    return 0;
}
