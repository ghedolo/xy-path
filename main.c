#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "audio_i2s.pio.h"
#include "renderer.h"
#include "paths_data.h"

#define I2S_DIN_PIN   4
#define I2S_BCLK_PIN  2
#define Z_PIN         5

#define PIO_INST  pio0
#define I2S_SM    0
#define Z_SM      1
#define AUDIO_DMA 0
#define Z_DMA     1

#define SAMPLE_RATE 96000

#define DEF_SCALE      32000.0f
#define DEF_FLYBACK     8
#define DEF_Z_OFFSET   18
#define DEF_DRAW_STEPS  2

static float g_scale = DEF_SCALE;

// Raw parsed paths (0..16384 coordinates)
typedef struct { int16_t pts[MAX_PTS][2]; int n; } raw_path_t;
static raw_path_t raw_paths[MAX_PATHS];
static int n_raw_paths = 0;

// Scaled paths (DAC values), rendered each frame
static path_t scaled_paths[MAX_PATHS];

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

static void parse_paths(void) {
    n_raw_paths = 0;
    const char *p = paths_data;
    while (*p && n_raw_paths < MAX_PATHS) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '#') { while (*p && *p != '\n') p++; continue; }
        if (*p != '[') { while (*p && *p != '\n') p++; continue; }
        p++; // skip '['
        raw_path_t *rp = &raw_paths[n_raw_paths];
        rp->n = 0;
        while (*p && *p != ']' && rp->n < MAX_PTS) {
            while (*p == ' ' || *p == ',') p++;
            if (*p == ']' || !*p) break;
            int x = (int)strtol(p, (char **)&p, 10);
            while (*p == ' ' || *p == ',') p++;
            int y = (int)strtol(p, (char **)&p, 10);
            rp->pts[rp->n][0] = (int16_t)(x < 0 ? 0 : x > 16384 ? 16384 : x);
            rp->pts[rp->n][1] = (int16_t)(y < 0 ? 0 : y > 16384 ? 16384 : y);
            rp->n++;
        }
        if (rp->n > 0) n_raw_paths++;
        while (*p && *p != '\n') p++;
    }
}

static void scale_paths(void) {
    for (int p = 0; p < n_raw_paths; p++) {
        scaled_paths[p].n = raw_paths[p].n;
        for (int i = 0; i < raw_paths[p].n; i++) {
            scaled_paths[p].pts[i][0] = (int16_t)((raw_paths[p].pts[i][0] - 8192) * g_scale / 8192.0f);
            scaled_paths[p].pts[i][1] = (int16_t)((raw_paths[p].pts[i][1] - 8192) * g_scale / 8192.0f);
        }
    }
}

// ---------------------------------------------------------------------------
// Double buffer
// ---------------------------------------------------------------------------

static frame_t frames[2];
static volatile int  playing_buf  = 0;
static volatile bool swap_pending = false;

static void dma_irq_handler(void) {
    if (!dma_channel_get_irq0_status(AUDIO_DMA)) return;
    dma_channel_acknowledge_irq0(AUDIO_DMA);
    playing_buf ^= 1;
    dma_channel_abort(Z_DMA);
    dma_channel_set_read_addr(Z_DMA,     frames[playing_buf].z,     true);
    dma_channel_set_read_addr(AUDIO_DMA, frames[playing_buf].audio, true);
    swap_pending = true;
}

// ---------------------------------------------------------------------------
// Hardware init
// ---------------------------------------------------------------------------

static void i2s_init(void) {
    PIO pio = PIO_INST; uint sm = I2S_SM;
    uint off = pio_add_program(pio, &audio_i2s_program);
    pio_sm_config c = audio_i2s_program_get_default_config(off);
    sm_config_set_out_pins(&c, I2S_DIN_PIN, 1);
    sm_config_set_sideset_pins(&c, I2S_BCLK_PIN);
    sm_config_set_out_shift(&c, false, true, 32);
    float clkdiv = (float)clock_get_hz(clk_sys) / (64.0f * SAMPLE_RATE);
    sm_config_set_clkdiv(&c, clkdiv);
    pio_gpio_init(pio, I2S_DIN_PIN);
    pio_gpio_init(pio, I2S_BCLK_PIN);
    pio_gpio_init(pio, I2S_BCLK_PIN + 1);
    for (uint pin = I2S_BCLK_PIN; pin <= I2S_BCLK_PIN + 1; pin++) {
        gpio_set_slew_rate(pin, GPIO_SLEW_RATE_SLOW);
        gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_2MA);
    }
    gpio_set_slew_rate(I2S_DIN_PIN, GPIO_SLEW_RATE_SLOW);
    gpio_set_drive_strength(I2S_DIN_PIN, GPIO_DRIVE_STRENGTH_2MA);
    pio_sm_set_consecutive_pindirs(pio, sm, I2S_DIN_PIN,  1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, I2S_BCLK_PIN, 2, true);
    pio_sm_init(pio, sm, off, &c);
    pio_sm_exec(pio, sm, pio_encode_set(pio_x, 14));
    pio_sm_set_enabled(pio, sm, true);
}

static void zaxis_init(void) {
    PIO pio = PIO_INST; uint sm = Z_SM;
    uint off = pio_add_program(pio, &zaxis_program);
    pio_sm_config c = zaxis_program_get_default_config(off);
    sm_config_set_out_pins(&c, Z_PIN, 1);
    sm_config_set_out_shift(&c, false, true, 32);
    float i2s_raw = (float)clock_get_hz(clk_sys) / (64.0f * SAMPLE_RATE);
    uint32_t i2s_int  = (uint32_t)i2s_raw;
    uint32_t i2s_frac = (uint32_t)((i2s_raw - i2s_int) * 256.0f + 0.5f);
    uint32_t z_total  = (i2s_int * 256 + i2s_frac) * 32;
    sm_config_set_clkdiv_int_frac(&c, z_total / 256, z_total % 256);
    gpio_init(Z_PIN); gpio_set_dir(Z_PIN, GPIO_OUT); gpio_put(Z_PIN, 0);
    pio_gpio_init(pio, Z_PIN);
    pio_sm_set_consecutive_pindirs(pio, sm, Z_PIN, 1, true);
    pio_sm_init(pio, sm, off, &c);
    pio_sm_set_enabled(pio, sm, true);
}

static void dma_init(void) {
    PIO pio = PIO_INST;
    {
        dma_channel_config c = dma_channel_get_default_config(AUDIO_DMA);
        channel_config_set_dreq(&c, pio_get_dreq(pio, I2S_SM, true));
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        dma_channel_configure(AUDIO_DMA, &c,
            (void *)&pio->txf[I2S_SM], frames[0].audio, SAMPLES_PER_FRAME, false);
        dma_channel_set_irq0_enabled(AUDIO_DMA, true);
    }
    {
        dma_channel_config c = dma_channel_get_default_config(Z_DMA);
        channel_config_set_dreq(&c, pio_get_dreq(pio, Z_SM, true));
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        dma_channel_configure(Z_DMA, &c,
            (void *)&pio->txf[Z_SM], frames[0].z, SAMPLES_PER_FRAME, false);
    }
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

static void print_help(void) {
    printf("gplog — ghedo 05/2026\n");
    printf("  +/-   z_offset  (%d, 0-60)\n",        z_offset);
    printf("  a/z   scale     (%.0f, 2000-32767)\n", g_scale);
    printf("  d/c   flyback   (%d, 1-40)\n",         flyback_steps);
    printf("  s/x   draw_steps(%d, 1-16)\n",         draw_steps);
    printf("  r     reset defaults\n");
    printf("  h     this help\n");
    printf("  paths: %d loaded\n", n_raw_paths);
}

int main(void) {
    stdio_init_all();

    parse_paths();
    scale_paths();

    render_paths(&frames[0], scaled_paths, n_raw_paths);
    render_paths(&frames[1], scaled_paths, n_raw_paths);

    i2s_init();
    zaxis_init();
    dma_init();

    dma_channel_start(Z_DMA);
    dma_channel_start(AUDIO_DMA);

    print_help();

    while (true) {
        int ch = getchar_timeout_us(0);
        bool changed = true;
        if (ch == 'h') {
            print_help();
            changed = false;
        } else if (ch == 'r') {
            z_offset      = DEF_Z_OFFSET;
            g_scale       = DEF_SCALE;
            flyback_steps = DEF_FLYBACK;
            draw_steps    = DEF_DRAW_STEPS;
            scale_paths();
            print_help();
        } else if (ch == '+' || ch == '=') {
            if (z_offset < 60) z_offset++;
        } else if (ch == '-') {
            if (z_offset > 0)  z_offset--;
        } else if (ch == 'a') {
            if (g_scale < 32767.0f) { g_scale += 500.0f; scale_paths(); }
        } else if (ch == 'z') {
            if (g_scale > 2000.0f)  { g_scale -= 500.0f; scale_paths(); }
        } else if (ch == 'd') {
            if (flyback_steps < 40) flyback_steps++;
        } else if (ch == 'c') {
            if (flyback_steps > 1)  flyback_steps--;
        } else if (ch == 's') {
            if (draw_steps < 16) draw_steps++;
        } else if (ch == 'x') {
            if (draw_steps > 1)  draw_steps--;
        } else {
            changed = false;
        }

        if (changed && ch != 'r' && ch != 'h') {
            printf("z=%d  sc=%.0f  fb=%d  ds=%d\n", z_offset, g_scale, flyback_steps, draw_steps);
        }

        if (swap_pending) {
            swap_pending = false;
            int back = 1 - playing_buf;
            render_paths(&frames[back], scaled_paths, n_raw_paths);
        }
    }
}
