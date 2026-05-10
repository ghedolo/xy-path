#pragma once
#include <stdint.h>

#define SAMPLES_PER_FRAME  1536
#define MAX_PATHS           64
#define MAX_PTS            128

typedef struct {
    uint32_t audio[SAMPLES_PER_FRAME];
    uint32_t z[SAMPLES_PER_FRAME];
} frame_t;

typedef struct {
    int16_t pts[MAX_PTS][2];  // DAC-scaled coords
    int     n;
} path_t;

extern volatile int z_offset;
extern volatile int flyback_steps;
extern volatile int draw_steps;

void render_paths(frame_t *f, const path_t *paths, int n_paths);
