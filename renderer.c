#include "renderer.h"
#include <string.h>
#include <stdbool.h>

#define Z_ON   0x80000000u
#define Z_OFF  0x00000000u

volatile int z_offset      = 18;
volatile int flyback_steps =  8;
volatile int draw_steps    =  2;

static inline uint32_t pack_xy(int32_t x, int32_t y) {
    if (x >  32767) x =  32767; else if (x < -32767) x = -32767;
    if (y >  32767) y =  32767; else if (y < -32767) y = -32767;
    return ((uint32_t)(uint16_t)(int16_t)y << 16) | (uint16_t)(int16_t)x;
}

static int write_segment(frame_t *f, int pos,
                          int32_t x0, int32_t y0,
                          int32_t x1, int32_t y1,
                          int steps, bool beam_on) {
    uint32_t zval = beam_on ? Z_ON : Z_OFF;
    for (int i = 0; i <= steps && pos < SAMPLES_PER_FRAME; i++, pos++) {
        int32_t x = x0 + (x1 - x0) * i / steps;
        int32_t y = y0 + (y1 - y0) * i / steps;
        f->audio[pos] = pack_xy(x, y);
        f->z[pos]     = zval;
    }
    return pos;
}

void render_paths(frame_t *f, const path_t *paths, int n_paths) {
    int guard = z_offset;
    if (guard < 0)  guard = 0;
    if (guard > 60) guard = 60;

    int pos = 0;
    int32_t cx = 0, cy = 0;

    for (int i = 0; i < 32; i++, pos++) {
        f->audio[pos] = pack_xy(0, 0);
        f->z[pos]     = Z_OFF;
    }

    for (int p = 0; p < n_paths && pos < SAMPLES_PER_FRAME; p++) {
        const path_t *path = &paths[p];
        if (path->n < 1) continue;

        int32_t px0 = path->pts[0][0];
        int32_t py0 = path->pts[0][1];

        if (px0 != cx || py0 != cy) {
            int fb = flyback_steps;
            if (fb < 1) fb = 1; if (fb > 40) fb = 40;
            pos = write_segment(f, pos, cx, cy, px0, py0, fb, false);
            pos = write_segment(f, pos, px0, py0, px0, py0, guard, false);
        }

        cx = px0; cy = py0;

        for (int i = 1; i < path->n && pos < SAMPLES_PER_FRAME; i++) {
            int32_t px1 = path->pts[i][0];
            int32_t py1 = path->pts[i][1];
            pos = write_segment(f, pos, cx, cy, px1, py1, draw_steps, true);
            cx = px1; cy = py1;
        }
    }

    while (pos < SAMPLES_PER_FRAME) {
        f->audio[pos] = pack_xy(cx, cy);
        f->z[pos]     = Z_OFF;
        pos++;
    }

    if (guard > 0) {
        memmove(f->z + guard, f->z, (SAMPLES_PER_FRAME - guard) * sizeof(uint32_t));
        for (int i = 0; i < guard; i++)
            f->z[i] = Z_OFF;
    }
}
