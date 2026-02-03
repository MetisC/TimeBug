#include "tapp.h"

#include "../../CORE/input.h"
#include "../../CORE/options.h"
#include "../../CORE/sound.h"
#include "../../CORE/video.h"
#include "../../CORE/sprite_dat.h"
#include "../../CORE/keyboard.h"
#include "../../CORE/high_scores.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <malloc.h>
#include <memory.h>

#define Y_WALL_TOP 28
#define Y_FLOOR_TOP 168
#define SCANLINE_STEP 7
#define PANEL_STEP 80
#define SHADOW_H 1

/* -------------------------------------------------------------------------
   COLORES DE FONDO
   ------------------------------------------------------------------------- */
#define COL_CEILING   1    // OJO: evita marrón para no confundir con barras
#define COL_WALL      153   // Ajusta si la paleta no cuadra
#define COL_FLOOR     5    // Debe ser más oscuro que la pared
#define COL_SEPARATOR 10   // Separadores

// Textura sutil cerca del color de pared
#define COL_SCANLINE  152
#define COL_PANELLINE 152

// Sombra un tono más oscuro que la pared
#define COL_SHADOW    17

#define TAP_TICKS_PER_SECOND 60
#define TAP_BAR_COUNT 3
#define TAP_BAR_W 256
#define TAP_BAR_H 24
#define TAP_BAR_X 32
#define TAP_BART_W 32
#define TAP_BART_H 48
#define TAP_CUST_W 24
#define TAP_CUST_H 40
#define TAP_BEER_W 8
#define TAP_BEER_H 12
#define TAP_MUG_W 8
#define TAP_MUG_H 12

#define TAP_BAR_BACK_ROWS 0
#define TAP_SPEED_GLOBAL 1.0f

#define TAP_MAX_CUSTOMERS_PER_BAR 4
#define TAP_MAX_BEERS_PER_BAR 2
#define TAP_MAX_MUGS_PER_BAR 4

#define TAP_SCORE_SERVE 100
#define TAP_SCORE_MUG 25
#define TAP_SCORE_WIN_BONUS 500

#define TAP_MUG_PICKUP_DISTANCE 10.0f
#define TAP_SERVE_X_TOLERANCE 0.5f

#define TAP_X_LEFT (TAP_BAR_X + 8)
#define TAP_X_RIGHT (TAP_BAR_X + TAP_BAR_W - 8)
#define TAP_FAIL_LINE (TAP_X_LEFT + 6)
#define TAP_BART_X_MIN (TAP_BAR_X - TAP_BART_W)
#define TAP_BART_X_MAX (TAP_BAR_X + TAP_BAR_W - TAP_BART_W - 8)
#define TAP_SPAWN_RETRY_COOLDOWN 4

typedef struct {
    int time_to_win_ticks;
    int max_customers_per_bar;
    int max_beers_in_flight_per_bar;
    int spawn_interval_ticks;
    float customer_speed;
    float beer_speed;
    float mug_speed;
    float bartender_speed;
    int serve_anim_ticks;
    int pickup_anim_ticks;
    int danger_dist_px;
} TapParams;

typedef struct {
    int spawn_initial_delay_ticks;
    int spawn_min_delay_ticks;
    int spawn_ramp_duration_ticks;
    int max_clients_initial;
    int max_clients_final;
    int lane_spawn_guard_dist;
    int spawn_retry_limit;
} TapSpawnParams;

typedef struct {
    unsigned short w;
    unsigned short h;
    unsigned long max_pixels;
    unsigned char far *pixels;
} TapSprite;

typedef enum {
    TAP_CUSTOMER_NONE = 0,
    TAP_CUSTOMER_ADVANCE,
    TAP_CUSTOMER_RETURN
} TapCustomerState;

typedef struct {
    float x;
    float x_prev;
    TapCustomerState state;
    unsigned char skin;
} TapCustomer;

typedef struct {
    float x;
    float x_prev;
    int active;
} TapProjectile;

typedef enum {
    TAP_BART_IDLE = 0,
    TAP_BART_SERVE,
    TAP_BART_PICKUP
} TapBartState;

static const int g_bar_y[TAP_BAR_COUNT] = {132, 96, 60};

static GameSettings g_settings;
static TapParams g_params;
static TapSpawnParams g_spawn_params;
static int g_finished = 0;
static int g_did_win = 0;
static int g_sound_enabled = 0;
static int g_use_keyboard = 1;

static TapSprite g_bar1 = {0, 0, TAP_BAR_W * TAP_BAR_H, NULL};
static TapSprite g_bart1 = {0, 0, TAP_BART_W * TAP_BART_H, NULL};
static TapSprite g_bart2 = {0, 0, TAP_BART_W * TAP_BART_H, NULL};
static TapSprite g_bart3 = {0, 0, TAP_BART_W * TAP_BART_H, NULL};
static TapSprite g_cust1 = {0, 0, TAP_CUST_W * TAP_CUST_H, NULL};
static TapSprite g_cust2 = {0, 0, TAP_CUST_W * TAP_CUST_H, NULL};
static TapSprite g_cust3 = {0, 0, TAP_CUST_W * TAP_CUST_H, NULL};
static TapSprite g_beer1 = {0, 0, TAP_BEER_W * TAP_BEER_H, NULL};
static TapSprite g_mug1 = {0, 0, TAP_MUG_W * TAP_MUG_H, NULL};
static int g_sprites_loaded = 0;
static int g_sprite_load_failed = 0;
static char g_sprite_fail_name[32] = {0};

static TapCustomer g_customers[TAP_BAR_COUNT][TAP_MAX_CUSTOMERS_PER_BAR];
static TapProjectile g_beers[TAP_BAR_COUNT][TAP_MAX_BEERS_PER_BAR];
static TapProjectile g_mugs[TAP_BAR_COUNT][TAP_MAX_MUGS_PER_BAR];

static float g_bartender_x = 0.0f;
static float g_bartender_x_prev = 0.0f;
static int g_bart_move_dir = 0;
static int g_active_bar = 0;
static TapBartState g_bart_state = TAP_BART_IDLE;
static int g_bart_anim_ticks = 0;
static int g_timer_ticks = 0;
static int g_spawn_cooldown_ticks = 0;
static int g_survived_ticks = 0;
static uint64_t g_score = 0;
static uint64_t g_final_score = 0;
static char g_end_detail[32] = "";
static char g_last_fail_reason[32] = "";
static int g_action_held = 0;
static int g_bar_switch_held = 0;

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int clampi(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float absf(float value)
{
    return value < 0.0f ? -value : value;
}

static void tapper_draw_background(void)
{
    int y;
    int x;
    int bar;

    v_fill_rect(0, 0, VIDEO_WIDTH, Y_WALL_TOP, COL_CEILING);
    v_fill_rect(0, Y_WALL_TOP, VIDEO_WIDTH, Y_FLOOR_TOP - Y_WALL_TOP, COL_WALL);
    v_fill_rect(0, Y_FLOOR_TOP, VIDEO_WIDTH, VIDEO_HEIGHT - Y_FLOOR_TOP, COL_FLOOR);

    v_fill_rect(0, Y_WALL_TOP, VIDEO_WIDTH, 1, COL_SEPARATOR);
    v_fill_rect(0, Y_FLOOR_TOP, VIDEO_WIDTH, 1, COL_SEPARATOR);

    // Tramado suave para textura CRT sin líneas sólidas
    for (y = Y_WALL_TOP + 2; y < Y_FLOOR_TOP; y += SCANLINE_STEP) {
        for (x = 0; x < VIDEO_WIDTH; x += 2) {
            v_putpixel(x, y, COL_SCANLINE);
        }
    }

    // Panelado vertical suave para evitar efecto reja
    for (x = 0; x < VIDEO_WIDTH; x += PANEL_STEP) {
        for (y = Y_WALL_TOP; y < Y_FLOOR_TOP; y += 3) {
            v_putpixel(x, y, COL_PANELLINE);
        }
    }

    // Sombra de 1px bajo las barras
    for (bar = 0; bar < TAP_BAR_COUNT; ++bar) {
        int shadow_y = g_bar_y[bar] + TAP_BAR_H;
        v_fill_rect(TAP_BAR_X, shadow_y, TAP_BAR_W, SHADOW_H, COL_SHADOW);
    }
}

static int lerp_int(int a, int b, float t)
{
    float value = (float)a + ((float)(b - a) * t);
    if (value >= 0.0f) {
        return (int)(value + 0.5f);
    }
    return (int)(value - 0.5f);
}

static int text_len(const char *text)
{
    int len = 0;

    if (!text) {
        return 0;
    }

    while (text[len] != '\0') {
        len++;
    }

    return len;
}

static const char *difficulty_short(unsigned char difficulty)
{
    switch (difficulty) {
    case DIFFICULTY_EASY:
        return "E";
    case DIFFICULTY_HARD:
        return "H";
    case DIFFICULTY_NORMAL:
    default:
        return "N";
    }
}

static void tap_select_params(unsigned char difficulty, TapParams *params)
{
    if (!params) {
        return;
    }

    switch (difficulty) {
    case DIFFICULTY_EASY:
        params->time_to_win_ticks = 20 * TAP_TICKS_PER_SECOND;
        params->max_customers_per_bar = 2;
        params->max_beers_in_flight_per_bar = 1;
        params->spawn_interval_ticks = 90;
        params->customer_speed = 0.25f;
        params->beer_speed = 1.25f;
        params->mug_speed = 0.65f;
        params->bartender_speed = 1.35f;
        params->serve_anim_ticks = 6;
        params->pickup_anim_ticks = 6;
        params->danger_dist_px = 18;
        g_spawn_params.spawn_initial_delay_ticks = 60;
        g_spawn_params.spawn_min_delay_ticks = 22;
        g_spawn_params.spawn_ramp_duration_ticks = 25 * TAP_TICKS_PER_SECOND;
        g_spawn_params.max_clients_initial = 2;
        g_spawn_params.max_clients_final = 5;
        g_spawn_params.lane_spawn_guard_dist = 32;
        g_spawn_params.spawn_retry_limit = 4;
        break;
    case DIFFICULTY_HARD:
        params->time_to_win_ticks = 30 * TAP_TICKS_PER_SECOND;
        params->max_customers_per_bar = 4;
        params->max_beers_in_flight_per_bar = 2;
        params->spawn_interval_ticks = 55;
        params->customer_speed = 0.38f;
        params->beer_speed = 1.55f;
        params->mug_speed = 0.90f;
        params->bartender_speed = 1.70f;
        params->serve_anim_ticks = 5;
        params->pickup_anim_ticks = 5;
        params->danger_dist_px = 24;
        g_spawn_params.spawn_initial_delay_ticks = 42;
        g_spawn_params.spawn_min_delay_ticks = 14;
        g_spawn_params.spawn_ramp_duration_ticks = 18 * TAP_TICKS_PER_SECOND;
        g_spawn_params.max_clients_initial = 3;
        g_spawn_params.max_clients_final = 7;
        g_spawn_params.lane_spawn_guard_dist = 28;
        g_spawn_params.spawn_retry_limit = 5;
        break;
    case DIFFICULTY_NORMAL:
    default:
        params->time_to_win_ticks = 25 * TAP_TICKS_PER_SECOND;
        params->max_customers_per_bar = 3;
        params->max_beers_in_flight_per_bar = 1;
        params->spawn_interval_ticks = 70;
        params->customer_speed = 0.30f;
        params->beer_speed = 1.35f;
        params->mug_speed = 0.75f;
        params->bartender_speed = 1.50f;
        params->serve_anim_ticks = 6;
        params->pickup_anim_ticks = 6;
        params->danger_dist_px = 20;
        g_spawn_params.spawn_initial_delay_ticks = 50;
        g_spawn_params.spawn_min_delay_ticks = 18;
        g_spawn_params.spawn_ramp_duration_ticks = 22 * TAP_TICKS_PER_SECOND;
        g_spawn_params.max_clients_initial = 3;
        g_spawn_params.max_clients_final = 6;
        g_spawn_params.lane_spawn_guard_dist = 30;
        g_spawn_params.spawn_retry_limit = 4;
        break;
    }

    params->customer_speed *= TAP_SPEED_GLOBAL;
    params->beer_speed *= TAP_SPEED_GLOBAL;
    params->mug_speed *= TAP_SPEED_GLOBAL;
    params->bartender_speed *= TAP_SPEED_GLOBAL;
}

static int tap_alloc_sprite(TapSprite *sprite)
{
    if (!sprite || sprite->max_pixels == 0) {
        return 0;
    }

    if (sprite->pixels) {
        return 1;
    }

    sprite->pixels = (unsigned char far *)_fmalloc(sprite->max_pixels);
    return sprite->pixels != NULL;
}

static void tap_free_sprite(TapSprite *sprite)
{
    if (!sprite || !sprite->pixels) {
        return;
    }

    _ffree(sprite->pixels);
    sprite->pixels = NULL;
    sprite->w = 0;
    sprite->h = 0;
}

static void tap_free_sprites(void)
{
    tap_free_sprite(&g_bar1);
    tap_free_sprite(&g_bart1);
    tap_free_sprite(&g_bart2);
    tap_free_sprite(&g_bart3);
    tap_free_sprite(&g_cust1);
    tap_free_sprite(&g_cust2);
    tap_free_sprite(&g_cust3);
    tap_free_sprite(&g_beer1);
    tap_free_sprite(&g_mug1);
    g_sprites_loaded = 0;
}

static void tap_note_sprite_fail(const char *name)
{
    int i = 0;

    g_sprite_load_failed = 1;
    while (name && name[i] && i < (int)sizeof(g_sprite_fail_name) - 1) {
        g_sprite_fail_name[i] = name[i];
        ++i;
    }
    g_sprite_fail_name[i] = '\0';
}

static int tap_read_sprite_header(FILE *file, unsigned short *out_w, unsigned short *out_h, long *out_header_size)
{
    long file_size;
    unsigned char h[4];
    size_t r;
    unsigned char w8, h8;
    unsigned short w16, h16;
    unsigned long size_ul;
    long expected;

    if (!file || !out_w || !out_h || !out_header_size) return 0;

    if (fseek(file, 0, SEEK_END) != 0) return 0;
    file_size = ftell(file);
    if (file_size < 0) return 0;
    if (fseek(file, 0, SEEK_SET) != 0) return 0;

    if (file_size >= 4) r = fread(h, 1, 4, file);
    else if (file_size >= 2) r = fread(h, 1, 2, file);
    else return 0;

    if (r < 2) return 0;

    // Prueba formato antiguo [w:1][h:1]
    w8 = h[0];
    h8 = h[1];
    size_ul = (unsigned long)w8 * (unsigned long)h8;
    expected = 2L + (long)size_ul;

    if (w8 > 0 && h8 > 0 && file_size == expected) {
        *out_w = (unsigned short)w8;
        *out_h = (unsigned short)h8;
        *out_header_size = 2L;
        return 1;
    }

    // Prueba formato nuevo [w:2][h:2] little-endian
    if (file_size >= 4 && r >= 4) {
        w16 = (unsigned short)h[0] | ((unsigned short)h[1] << 8);
        h16 = (unsigned short)h[2] | ((unsigned short)h[3] << 8);
        size_ul = (unsigned long)w16 * (unsigned long)h16;
        expected = 4L + (long)size_ul;

        if (w16 > 0 && h16 > 0 && file_size == expected) {
            *out_w = w16;
            *out_h = h16;
            *out_header_size = 4L;
            return 1;
        }
    }

    return 0;
}

static int tap_load_sprite(const char *path, TapSprite *sprite)
{
    FILE *file;
    unsigned short w, h;
    unsigned long size_ul;
    long header_size;
    unsigned long remaining;
    unsigned long offset;
    unsigned char buf[1024];
    size_t chunk;

    if (!sprite || !path) return 0;

    if (!sprite->pixels && !tap_alloc_sprite(sprite)) return 0;

    sprite->w = sprite->h = 0;

    file = fopen(path, "rb");
    if (!file) return 0;

    if (!tap_read_sprite_header(file, &w, &h, &header_size)) {
        fclose(file);
        return 0;
    }

    size_ul = (unsigned long)w * (unsigned long)h;
    if (size_ul == 0 || size_ul > sprite->max_pixels) {
        fclose(file);
        return 0;
    }

    if (fseek(file, header_size, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    remaining = size_ul;
    offset = 0;
    while (remaining > 0) {
        chunk = (remaining > sizeof(buf)) ? sizeof(buf) : (size_t)remaining;
        if (fread(buf, 1, chunk, file) != chunk) {
            fclose(file);
            sprite->w = sprite->h = 0;
            return 0;
        }
        _fmemcpy(sprite->pixels + offset, buf, chunk);
        offset += (unsigned long)chunk;
        remaining -= (unsigned long)chunk;
    }

    fclose(file);
    sprite->w = w;
    sprite->h = h;
    return 1;
}

static void tap_load_sprites(void)
{
    int ok = 1;

    if (g_sprites_loaded) {
        return;
    }

    g_sprite_load_failed = 0;
    g_sprite_fail_name[0] = '\0';

    ok &= tap_load_sprite("SPRITES\\bar1.dat", &g_bar1);
    if (!ok) {
        tap_note_sprite_fail("bar1.dat");
        tap_free_sprites();
        return;
    }
    ok &= tap_load_sprite("SPRITES\\bart1.dat", &g_bart1);
    if (!ok) {
        tap_note_sprite_fail("bart1.dat");
        tap_free_sprites();
        return;
    }
    ok &= tap_load_sprite("SPRITES\\bart2.dat", &g_bart2);
    if (!ok) {
        tap_note_sprite_fail("bart2.dat");
        tap_free_sprites();
        return;
    }
    ok &= tap_load_sprite("SPRITES\\bart3.dat", &g_bart3);
    if (!ok) {
        tap_note_sprite_fail("bart3.dat");
        tap_free_sprites();
        return;
    }
    ok &= tap_load_sprite("SPRITES\\cust1.dat", &g_cust1);
    if (!ok) {
        tap_note_sprite_fail("cust1.dat");
        tap_free_sprites();
        return;
    }
    ok &= tap_load_sprite("SPRITES\\cust2.dat", &g_cust2);
    if (!ok) {
        tap_note_sprite_fail("cust2.dat");
        tap_free_sprites();
        return;
    }
    ok &= tap_load_sprite("SPRITES\\cust3.dat", &g_cust3);
    if (!ok) {
        tap_note_sprite_fail("cust3.dat");
        tap_free_sprites();
        return;
    }
    ok &= tap_load_sprite("SPRITES\\beer1.dat", &g_beer1);
    if (!ok) {
        tap_note_sprite_fail("beer1.dat");
        tap_free_sprites();
        return;
    }
    ok &= tap_load_sprite("SPRITES\\mug1.dat", &g_mug1);
    if (!ok) {
        tap_note_sprite_fail("mug1.dat");
        tap_free_sprites();
        return;
    }

    g_sprites_loaded = 1;
}

static void tap_blit_sprite(int x, int y, const TapSprite *sprite)
{
    if (!sprite || sprite->w == 0 || sprite->h == 0) {
        return;
    }

    v_blit_sprite(x, y, sprite->w, sprite->h, sprite->pixels, 0);
}

static const TapSprite *tap_customer_sprite(unsigned char skin)
{
    switch (skin % 3) {
    case 1:
        return &g_cust2;
    case 2:
        return &g_cust3;
    default:
        return &g_cust1;
    }
}

static void tap_blit_sprite_flipped(int x, int y, const TapSprite *sprite)
{
    int sx;
    int sy;
    int w;
    int h;

    if (!sprite || !sprite->pixels || sprite->w == 0 || sprite->h == 0) {
        return;
    }

    w = (int)sprite->w;
    h = (int)sprite->h;

    for (sy = 0; sy < h; ++sy) {
        int dy = y + sy;
        if (dy < 0 || dy >= VIDEO_HEIGHT) {
            continue;
        }
        for (sx = 0; sx < w; ++sx) {
            int dx = x + sx;
            unsigned char c;
            if (dx < 0 || dx >= VIDEO_WIDTH) {
                continue;
            }
            c = sprite->pixels[sy * w + (w - 1 - sx)];
            if (c != 0) {
                v_putpixel(dx, dy, c);
            }
        }
    }
}

static void tap_blit_sprite_rows(int x, int y, const TapSprite *sprite, int src_y, int rows)
{
    int sx;
    int sy;
    int w;
    int h;

    if (!sprite || !sprite->pixels || sprite->w == 0 || sprite->h == 0) {
        return;
    }

    w = (int)sprite->w;
    h = (int)sprite->h;

    if (src_y < 0) {
        src_y = 0;
    }
    if (rows <= 0) {
        return;
    }
    if (src_y >= h) {
        return;
    }
    if (src_y + rows > h) {
        rows = h - src_y;
    }

    for (sy = 0; sy < rows; ++sy) {
        int py = src_y + sy;
        for (sx = 0; sx < w; ++sx) {
            unsigned char c = sprite->pixels[py * w + sx];
            if (c != 0) {
                v_putpixel(x + sx, y + sy, c);
            }
        }
    }
}

static void tap_draw_placeholder(float alpha, float bartender_x, TapBartState bart_state)
{
    int bar;
    int i;

    v_fill_rect(0, 16, VIDEO_WIDTH, 168, 1);

    for (bar = TAP_BAR_COUNT - 1; bar >= 0; --bar) {
        int y_customer = g_bar_y[bar] - 18;

        v_fill_rect(TAP_BAR_X, g_bar_y[bar], TAP_BAR_W, TAP_BAR_BACK_ROWS, 6);

        for (i = 0; i < TAP_MAX_CUSTOMERS_PER_BAR; ++i) {
            TapCustomer *cust = &g_customers[bar][i];
            if (cust->state == TAP_CUSTOMER_NONE) {
                continue;
            }
            {
                float cx = cust->x_prev + (cust->x - cust->x_prev) * alpha;
                unsigned char color = 10;
                if (cust->state == TAP_CUSTOMER_RETURN) {
                    color = 12;
                } else if (cust->state == TAP_CUSTOMER_ADVANCE) {
                    if (cust->x <= (float)(TAP_FAIL_LINE + g_params.danger_dist_px)) {
                        color = 4;
                    }
                }
                v_fill_rect((int)cx, y_customer, TAP_CUST_W, TAP_CUST_H, color);
            }
        }

        if (bar == g_active_bar) {
            int y_bart = g_bar_y[bar] - 24;
            unsigned char color = 14;
            if (bart_state == TAP_BART_SERVE) {
                color = 11;
            } else if (bart_state == TAP_BART_PICKUP) {
                color = 7;
            }
            v_fill_rect((int)bartender_x, y_bart, TAP_BART_W, TAP_BART_H, color);
        }

        v_fill_rect(TAP_BAR_X, g_bar_y[bar] + TAP_BAR_BACK_ROWS, TAP_BAR_W, TAP_BAR_H - TAP_BAR_BACK_ROWS, 6);
    }

    for (bar = 0; bar < TAP_BAR_COUNT; ++bar) {
        int y_surface = g_bar_y[bar] - 3;
        for (i = 0; i < TAP_MAX_BEERS_PER_BAR; ++i) {
            TapProjectile *beer = &g_beers[bar][i];
            if (!beer->active) {
                continue;
            }
            {
                float bx = beer->x_prev + (beer->x - beer->x_prev) * alpha;
                v_fill_rect((int)bx, y_surface, TAP_BEER_W, TAP_BEER_H, 11);
            }
        }
        for (i = 0; i < TAP_MAX_MUGS_PER_BAR; ++i) {
            TapProjectile *mug = &g_mugs[bar][i];
            if (!mug->active) {
                continue;
            }
            {
                float mx = mug->x_prev + (mug->x - mug->x_prev) * alpha;
                v_fill_rect((int)mx, y_surface, TAP_MUG_W, TAP_MUG_H, 7);
            }
        }
    }
}

static int tap_spawn_customer(int bar)
{
    int i;

    for (i = 0; i < TAP_MAX_CUSTOMERS_PER_BAR; ++i) {
        TapCustomer *cust = &g_customers[bar][i];
        if (cust->state == TAP_CUSTOMER_NONE) {
            cust->x = (float)(TAP_X_RIGHT - TAP_CUST_W);
            cust->x_prev = cust->x;
            cust->state = TAP_CUSTOMER_ADVANCE;
            cust->skin = (unsigned char)(rand() % 3);
            return 1;
        }
    }

    return 0;
}

static int tap_lane_has_space(int bar, float spawn_x, int guard_dist)
{
    int i;

    for (i = 0; i < TAP_MAX_CUSTOMERS_PER_BAR; ++i) {
        TapCustomer *cust = &g_customers[bar][i];
        if (cust->state == TAP_CUSTOMER_NONE) {
            continue;
        }
        if (absf(cust->x - spawn_x) < (float)guard_dist) {
            return 0;
        }
    }

    return 1;
}

static int tap_try_spawn_customer(int guard_dist, int retry_limit)
{
    int attempts = retry_limit > 0 ? retry_limit : 1;
    int i;
    float spawn_x = (float)(TAP_X_RIGHT - TAP_CUST_W);

    for (i = 0; i < attempts; ++i) {
        int bar = rand() % TAP_BAR_COUNT;
        if (!tap_lane_has_space(bar, spawn_x, guard_dist)) {
            continue;
        }
        if (tap_spawn_customer(bar)) {
            return 1;
        }
    }

    return 0;
}

static int tap_spawn_beer(int bar, float bartender_x)
{
    int i;
    float start_x = bartender_x + (TAP_BART_W * 0.5f) - (TAP_BEER_W * 0.5f);

    for (i = 0; i < TAP_MAX_BEERS_PER_BAR; ++i) {
        TapProjectile *beer = &g_beers[bar][i];
        if (!beer->active) {
            beer->active = 1;
            beer->x = start_x;
            beer->x_prev = beer->x;
            return 1;
        }
    }

    return 0;
}

static int tap_spawn_mug(int bar, float start_x)
{
    int i;

    for (i = 0; i < TAP_MAX_MUGS_PER_BAR; ++i) {
        TapProjectile *mug = &g_mugs[bar][i];
        if (!mug->active) {
            mug->active = 1;
            mug->x = start_x;
            mug->x_prev = mug->x;
            return 1;
        }
    }

    return 0;
}

static int tap_pickup_mug(int bar, float bartender_x)
{
    int i;

    for (i = 0; i < TAP_MAX_MUGS_PER_BAR; ++i) {
        TapProjectile *mug = &g_mugs[bar][i];
        if (mug->active) {
            float bart_left = bartender_x - TAP_MUG_PICKUP_DISTANCE;
            float bart_right = bartender_x + TAP_BART_W + TAP_MUG_PICKUP_DISTANCE;
            float mug_left = mug->x;
            float mug_right = mug->x + TAP_MUG_W;
            if (mug_left <= bart_right && mug_right >= bart_left) {
                mug->active = 0;
                return 1;
            }
        }
    }

    return 0;
}

static void tap_fail(const char *reason)
{
    g_finished = 1;
    g_did_win = 0;
    if (reason) {
        snprintf(g_last_fail_reason, sizeof(g_last_fail_reason), "%s", reason);
    }
}

void Tapp_Init(const GameSettings *settings)
{
    int bar;
    int i;

    if (settings) {
        g_settings = *settings;
    } else {
        memset(&g_settings, 0, sizeof(g_settings));
    }

    tap_select_params(g_settings.difficulty, &g_params);
    g_params.customer_speed *= g_settings.speed_multiplier;
    g_params.beer_speed *= g_settings.speed_multiplier;
    g_params.mug_speed *= g_settings.speed_multiplier;
    g_params.bartender_speed *= g_settings.speed_multiplier;

    tap_load_sprites();
    if (!g_sprites_loaded) {
        g_sprite_load_failed = 1;
    }

    g_finished = 0;
    g_did_win = 0;
    g_sound_enabled = g_settings.sound_enabled ? 1 : 0;
    g_use_keyboard = 1;
    if (g_settings.input_mode == INPUT_JOYSTICK) {
        if (in_joystick_available()) {
            g_use_keyboard = 0;
        }
    }

    g_bartender_x = (float)TAP_BART_X_MIN;
    g_bartender_x_prev = g_bartender_x;
    g_bart_move_dir = 0;
    g_active_bar = 0;
    g_bart_state = TAP_BART_IDLE;
    g_bart_anim_ticks = 0;
    g_timer_ticks = g_params.time_to_win_ticks;
    g_score = 0;
    g_final_score = 0;
    g_end_detail[0] = '\0';
    g_last_fail_reason[0] = '\0';
    g_action_held = 0;
    g_bar_switch_held = 0;
    g_survived_ticks = 0;
    g_spawn_cooldown_ticks = g_spawn_params.spawn_initial_delay_ticks;

    for (bar = 0; bar < TAP_BAR_COUNT; ++bar) {
        for (i = 0; i < TAP_MAX_CUSTOMERS_PER_BAR; ++i) {
            g_customers[bar][i].state = TAP_CUSTOMER_NONE;
            g_customers[bar][i].x = 0.0f;
            g_customers[bar][i].x_prev = 0.0f;
            g_customers[bar][i].skin = 0;
        }
        for (i = 0; i < TAP_MAX_BEERS_PER_BAR; ++i) {
            g_beers[bar][i].active = 0;
            g_beers[bar][i].x = 0.0f;
            g_beers[bar][i].x_prev = 0.0f;
        }
        for (i = 0; i < TAP_MAX_MUGS_PER_BAR; ++i) {
            g_mugs[bar][i].active = 0;
            g_mugs[bar][i].x = 0.0f;
            g_mugs[bar][i].x_prev = 0.0f;
        }
    }

    while (in_keyhit()) {
        in_poll();
    }
}

void Tapp_StorePreviousState(void)
{
    int bar;
    int i;

    g_bartender_x_prev = g_bartender_x;

    for (bar = 0; bar < TAP_BAR_COUNT; ++bar) {
        for (i = 0; i < TAP_MAX_CUSTOMERS_PER_BAR; ++i) {
            g_customers[bar][i].x_prev = g_customers[bar][i].x;
        }
        for (i = 0; i < TAP_MAX_BEERS_PER_BAR; ++i) {
            g_beers[bar][i].x_prev = g_beers[bar][i].x;
        }
        for (i = 0; i < TAP_MAX_MUGS_PER_BAR; ++i) {
            g_mugs[bar][i].x_prev = g_mugs[bar][i].x;
        }
    }
}

void Tapp_Update(void)
{
    int bar;
    int i;
    int move_dir = 0;
    int action_down = 0;
    int action_pressed = 0;
    int bar_delta = 0;
    int active_customers = 0;

    if (g_finished) {
        return;
    }

    sound_update();
    g_survived_ticks++;

    if (g_use_keyboard) {
        if (kb_down(SC_LEFT)) {
            move_dir -= 1;
        }
        if (kb_down(SC_RIGHT)) {
            move_dir += 1;
        }
        if (kb_down(SC_UP)) {
            bar_delta += 1;
        }
        if (kb_down(SC_DOWN)) {
            bar_delta -= 1;
        }
        if (kb_down(SC_SPACE) || kb_down(SC_LCTRL)) {
            action_down = 1;
        }
    } else {
        int dx = 0;
        int dy = 0;
        unsigned char buttons = 0;

        if (in_joystick_direction(&dx, &dy, &buttons)) {
            move_dir = dx;
            if (dy < 0) {
                bar_delta = 1;
            } else if (dy > 0) {
                bar_delta = -1;
            }
            if (buttons & 1) {
                action_down = 1;
            }
        }
    }

    action_pressed = action_down && !g_action_held;
    g_action_held = action_down;

    if (bar_delta != 0 && !g_bar_switch_held) {
        g_active_bar += bar_delta;
        if (g_active_bar < 0) {
            g_active_bar = 0;
        } else if (g_active_bar >= TAP_BAR_COUNT) {
            g_active_bar = TAP_BAR_COUNT - 1;
        }
        g_bar_switch_held = 1;
    } else if (bar_delta == 0) {
        g_bar_switch_held = 0;
    }

    g_bartender_x += (float)move_dir * g_params.bartender_speed;
    g_bartender_x = clampf(g_bartender_x, (float)TAP_BART_X_MIN, (float)TAP_BART_X_MAX);
    g_bart_move_dir = move_dir;

    if (g_bart_anim_ticks > 0) {
        g_bart_anim_ticks--;
        if (g_bart_anim_ticks <= 0) {
            g_bart_state = TAP_BART_IDLE;
        }
    }

    {
        int picked = tap_pickup_mug(g_active_bar, g_bartender_x);
        if (picked) {
            g_score += TAP_SCORE_MUG;
            g_bart_state = TAP_BART_PICKUP;
            g_bart_anim_ticks = g_params.pickup_anim_ticks;
            if (g_sound_enabled) {
                sound_play_tone(380, 25);
            }
        }
        if (action_pressed && !picked) {
            if (g_bartender_x <= (float)TAP_BART_X_MIN + TAP_SERVE_X_TOLERANCE) {
                if (tap_spawn_beer(g_active_bar, g_bartender_x)) {
                    g_bart_state = TAP_BART_SERVE;
                    g_bart_anim_ticks = g_params.serve_anim_ticks;
                    if (g_sound_enabled) {
                        sound_play_tone(520, 20);
                    }
                }
            }
        }
    }

    for (bar = 0; bar < TAP_BAR_COUNT; ++bar) {
        for (i = 0; i < TAP_MAX_CUSTOMERS_PER_BAR; ++i) {
            if (g_customers[bar][i].state != TAP_CUSTOMER_NONE) {
                active_customers++;
            }
        }
    }

    {
        float ramp_progress = 1.0f;
        int current_max_clients;
        int current_spawn_delay;
        int max_min = g_spawn_params.max_clients_initial;
        int max_max = g_spawn_params.max_clients_final;
        int delay_min = g_spawn_params.spawn_min_delay_ticks;
        int delay_max = g_spawn_params.spawn_initial_delay_ticks;

        if (g_spawn_params.spawn_ramp_duration_ticks > 0) {
            ramp_progress = clampf((float)g_survived_ticks / (float)g_spawn_params.spawn_ramp_duration_ticks, 0.0f, 1.0f);
        }

        current_max_clients = lerp_int(g_spawn_params.max_clients_initial, g_spawn_params.max_clients_final, ramp_progress);
        current_spawn_delay = lerp_int(g_spawn_params.spawn_initial_delay_ticks, g_spawn_params.spawn_min_delay_ticks, ramp_progress);
        if (max_min > max_max) {
            int tmp = max_min;
            max_min = max_max;
            max_max = tmp;
        }
        if (delay_min > delay_max) {
            int tmp = delay_min;
            delay_min = delay_max;
            delay_max = tmp;
        }
        current_max_clients = clampi(current_max_clients, max_min, max_max);
        current_spawn_delay = clampi(current_spawn_delay, delay_min, delay_max);

        if (g_spawn_cooldown_ticks > 0) {
            g_spawn_cooldown_ticks--;
        }

        if (g_spawn_cooldown_ticks <= 0) {
            if (active_customers < current_max_clients) {
                if (tap_try_spawn_customer(g_spawn_params.lane_spawn_guard_dist, g_spawn_params.spawn_retry_limit)) {
                    g_spawn_cooldown_ticks = current_spawn_delay;
                } else {
                    g_spawn_cooldown_ticks = TAP_SPAWN_RETRY_COOLDOWN;
                }
            } else {
                g_spawn_cooldown_ticks = TAP_SPAWN_RETRY_COOLDOWN;
            }
        }
    }

    for (bar = 0; bar < TAP_BAR_COUNT; ++bar) {
        for (i = 0; i < TAP_MAX_CUSTOMERS_PER_BAR; ++i) {
            TapCustomer *cust = &g_customers[bar][i];
            if (cust->state == TAP_CUSTOMER_ADVANCE) {
                cust->x -= g_params.customer_speed;
                if (cust->x <= (float)TAP_FAIL_LINE) {
                    tap_fail("CLIENTE");
                    return;
                }
            } else if (cust->state == TAP_CUSTOMER_RETURN) {
                cust->x += g_params.customer_speed;
                if (cust->x >= (float)(TAP_X_RIGHT - TAP_CUST_W)) {
                    cust->state = TAP_CUSTOMER_NONE;
                }
            }
        }

        for (i = 0; i < TAP_MAX_BEERS_PER_BAR; ++i) {
            TapProjectile *beer = &g_beers[bar][i];
            if (!beer->active) {
                continue;
            }
            beer->x += g_params.beer_speed;
            if (beer->x > (float)TAP_X_RIGHT) {
                beer->active = 0;
                tap_fail("JARRA");
                return;
            }

            {
                int c;
                for (c = 0; c < TAP_MAX_CUSTOMERS_PER_BAR; ++c) {
                    TapCustomer *cust = &g_customers[bar][c];
                    if (cust->state != TAP_CUSTOMER_ADVANCE) {
                        continue;
                    }
                    if ((beer->x + TAP_BEER_W) >= cust->x && beer->x <= (cust->x + TAP_CUST_W)) {
                        cust->state = TAP_CUSTOMER_RETURN;
                        tap_spawn_mug(bar, cust->x + (TAP_CUST_W * 0.5f) - (TAP_MUG_W * 0.5f));
                        beer->active = 0;
                        g_score += TAP_SCORE_SERVE;
                        if (g_sound_enabled) {
                            sound_play_tone(620, 25);
                        }
                        break;
                    }
                }
            }
        }

        for (i = 0; i < TAP_MAX_MUGS_PER_BAR; ++i) {
            TapProjectile *mug = &g_mugs[bar][i];
            if (!mug->active) {
                continue;
            }
            mug->x -= g_params.mug_speed;
            if (mug->x < (float)(TAP_X_LEFT - TAP_MUG_W)) {
                tap_fail("VASO");
                return;
            }
        }

    }

    g_timer_ticks--;
    if (g_timer_ticks <= 0) {
        g_timer_ticks = 0;
        g_finished = 1;
        g_did_win = 1;
        return;
    }
}

void Tapp_DrawInterpolated(float alpha)
{
    char hud[64];
    char timer_text[24];
    char score_text[24];
    int bar;
    int i;
    float bartender_x = g_bartender_x_prev + (g_bartender_x - g_bartender_x_prev) * alpha;
    TapBartState bart_state = g_bart_state;

    tapper_draw_background();

    if (!g_sprites_loaded) {
        tap_draw_placeholder(alpha, bartender_x, bart_state);
        if (g_sprite_load_failed) {
            v_puts(4, 4, "TAPPER: sprites NOT loaded", 15);
            if (g_sprite_fail_name[0] != '\0') {
                v_puts(4, 12, "fail:", 15);
                v_puts(52, 12, g_sprite_fail_name, 15);
            }
        }
    } else {

        for (bar = TAP_BAR_COUNT - 1; bar >= 0; --bar) {
            tap_blit_sprite_rows(TAP_BAR_X, g_bar_y[bar], &g_bar1, 0, TAP_BAR_BACK_ROWS);
        }

        for (bar = TAP_BAR_COUNT - 1; bar >= 0; --bar) {
            int y_customer = g_bar_y[bar] - 18;
            for (i = 0; i < TAP_MAX_CUSTOMERS_PER_BAR; ++i) {
                TapCustomer *cust = &g_customers[bar][i];
                if (cust->state == TAP_CUSTOMER_NONE) {
                    continue;
                }
                {
                    float cx = cust->x_prev + (cust->x - cust->x_prev) * alpha;
                    const TapSprite *sprite = tap_customer_sprite(cust->skin);
                    if (cust->state == TAP_CUSTOMER_RETURN) {
                        tap_blit_sprite_flipped((int)cx, y_customer, sprite);
                    } else {
                        tap_blit_sprite((int)cx, y_customer, sprite);
                    }
                }
            }

            if (bar == g_active_bar) {
                int y_bart = g_bar_y[bar] - 24;
                const TapSprite *sprite = &g_bart1;
                int flip = 0;
                if (bart_state == TAP_BART_SERVE) {
                    sprite = &g_bart2;
                } else if (g_bart_move_dir != 0) {
                    sprite = &g_bart3;
                    if (g_bart_move_dir < 0) {
                        flip = 1;
                    }
                }
                if (flip) {
                    tap_blit_sprite_flipped((int)bartender_x, y_bart, sprite);
                } else {
                    tap_blit_sprite((int)bartender_x, y_bart, sprite);
                }
            }

            tap_blit_sprite_rows(TAP_BAR_X, g_bar_y[bar] + TAP_BAR_BACK_ROWS, &g_bar1, TAP_BAR_BACK_ROWS,
                                 (int)g_bar1.h - TAP_BAR_BACK_ROWS);
        }

        for (bar = 0; bar < TAP_BAR_COUNT; ++bar) {
            int y_surface = g_bar_y[bar] - 3;
            for (i = 0; i < TAP_MAX_BEERS_PER_BAR; ++i) {
                TapProjectile *beer = &g_beers[bar][i];
                if (!beer->active) {
                    continue;
                }
                {
                    float bx = beer->x_prev + (beer->x - beer->x_prev) * alpha;
                    tap_blit_sprite((int)bx, y_surface, &g_beer1);
                }
            }
            for (i = 0; i < TAP_MAX_MUGS_PER_BAR; ++i) {
                TapProjectile *mug = &g_mugs[bar][i];
                if (!mug->active) {
                    continue;
                }
                {
                    float mx = mug->x_prev + (mug->x - mug->x_prev) * alpha;
                    tap_blit_sprite((int)mx, y_surface, &g_mug1);
                }
            }
        }
    }

    v_fill_rect(0, 0, VIDEO_WIDTH, 16, 0);
    v_fill_rect(0, 184, VIDEO_WIDTH, 16, 0);

    snprintf(hud, sizeof(hud), "D:%s x%.2f", difficulty_short(g_settings.difficulty), g_settings.speed_multiplier);
    v_puts(0, 0, "1983", 7);
    v_puts(VIDEO_WIDTH - (text_len(hud) * 8), 0, hud, 7);

    snprintf(timer_text, sizeof(timer_text), "TIEMPO %d", g_timer_ticks / TAP_TICKS_PER_SECOND);
    v_puts(8, 188, timer_text, 15);

    high_scores_format_score(score_text, sizeof(score_text), g_score);
    v_puts(VIDEO_WIDTH - (text_len(score_text) * 8) - 8, 188, score_text, 15);

    v_present();
}

void Tapp_End(void)
{
    if (g_did_win) {
        char score_text[16];
        g_final_score = g_score + TAP_SCORE_WIN_BONUS;
        high_scores_format_score(score_text, sizeof(score_text), g_final_score);
        snprintf(g_end_detail, sizeof(g_end_detail), "PUNTOS %s", score_text);
    } else {
        if (g_last_fail_reason[0] != '\0') {
            snprintf(g_end_detail, sizeof(g_end_detail), "FALLO %s", g_last_fail_reason);
        } else {
            snprintf(g_end_detail, sizeof(g_end_detail), "FALLO");
        }
        g_final_score = g_score;
    }

    tap_free_sprites();
}

int Tapp_IsFinished(void)
{
    return g_finished;
}

int Tapp_DidWin(void)
{
    return g_did_win;
}

const char *Tapp_GetEndDetail(void)
{
    return g_end_detail;
}

uint64_t Tapp_GetScore(void)
{
    return g_final_score;
}
