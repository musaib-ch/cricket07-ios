#include "ViewportBridge.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int close_to(double a, double b) { return fabs(a - b) < 0.0001; }

int main(void) {
    CmdDraw draw;
    memset(&draw, 0, sizeof(draw));
    Cricket07Viewport v = {0};
    draw.minz = 0.2f;
    draw.maxz = 0.8f;
    draw.vp[0] = 10.f; draw.vp[1] = 20.f;
    draw.vp[2] = 100.f; draw.vp[3] = 50.f;
    assert(cricket07_resolve_viewport(&draw, 1280, 960, 640, 480, &v));
    assert(close_to(v.x, 20) && close_to(v.y, 40));
    assert(close_to(v.width, 200) && close_to(v.height, 100));
    assert(close_to(v.min_depth, 0.2) && close_to(v.max_depth, 0.8));

    draw.vp[0] = 600.f; draw.vp[1] = 450.f;
    assert(cricket07_resolve_viewport(&draw, 1280, 960, 640, 480, &v));
    assert(close_to(v.x, 1200) && close_to(v.y, 900));
    assert(close_to(v.width, 80) && close_to(v.height, 60));

    draw.vp[0] = -10.f; draw.vp[1] = -5.f;
    assert(cricket07_resolve_viewport(&draw, 1280, 960, 640, 480, &v));
    assert(close_to(v.x, 0) && close_to(v.y, 0));
    assert(close_to(v.width, 180) && close_to(v.height, 90));

    draw.full_viewport = 1;
    draw.vp[2] = 0;
    assert(cricket07_resolve_viewport(&draw, 1280, 960, 640, 480, &v));
    assert(close_to(v.width, 1280) && close_to(v.height, 960));
    draw.full_viewport = 0;
    assert(!cricket07_resolve_viewport(&draw, 1280, 960, 640, 480, &v));
    draw.vp[2] = 100.f;
    draw.vp[0] = NAN;
    assert(!cricket07_resolve_viewport(&draw, 1280, 960, 640, 480, &v));
    draw.vp[0] = 1000.f;
    assert(!cricket07_resolve_viewport(&draw, 1280, 960, 640, 480, &v));
    draw.vp[0] = 0;
    draw.minz = 0.9f;
    assert(!cricket07_resolve_viewport(&draw, 1280, 960, 640, 480, &v));
    draw.minz = 0;
    draw.maxz = 1;
    assert(!cricket07_resolve_viewport(&draw, 0, 960, 640, 480, &v));
    assert(!cricket07_resolve_viewport(&draw, 1280, 960, 0, 480, &v));
    assert(!cricket07_resolve_viewport(NULL, 1280, 960, 640, 480, &v));
    assert(!cricket07_resolve_viewport(&draw, 1280, 960, 640, 480, NULL));
    puts("iOS viewport bridge regression passed");
    return 0;
}
