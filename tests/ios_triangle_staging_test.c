#include "TriangleStaging.h"
#include "render_cmd.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
int main(void) {
    struct { CmdDraw header; CVert vertices[3]; } cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.header.h.type = CMD_DRAW;
    cmd.header.h.size = sizeof(cmd);
    cmd.header.key.topo = 4;
    cmd.header.nverts = 3;
    cmd.vertices[0].x = -1.0f;
    cmd.vertices[1].x = 1.0f;
    cmd.vertices[2].y = 1.0f;
    float coordinates[12] = {0};
    size_t count = 0;
    assert(cricket07_extract_triangles(&cmd, sizeof(cmd), coordinates, 12, &count));
    assert(count == 3 && coordinates[0] == -1.0f && coordinates[4] == 1.0f && coordinates[9] == 1.0f);
    assert(!cricket07_extract_triangles(&cmd, sizeof(cmd)-1, coordinates, 12, &count));
    assert(!cricket07_extract_triangles(&cmd, sizeof(cmd), coordinates, 11, &count));
    cmd.header.key.topo = 2;
    assert(!cricket07_extract_triangles(&cmd, sizeof(cmd), coordinates, 12, &count));
    puts("Triangle staging tests passed");
    return 0;
}
