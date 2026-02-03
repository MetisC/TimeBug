#include "gori.h"

#include "../../CORE/input.h"
#include "../../CORE/options.h"
#include "../../CORE/sound.h"
#include "../../CORE/video.h"
#include "../../CORE/keyboard.h"
#include "../../CORE/high_scores.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define GORI_MIN_BUILDING_W 14
#define GORI_MAX_BUILDING_W 30
#define GORI_MIN_BUILDING_H 40
#define GORI_MAX_BUILDING_H 120
#define GORI_MAX_BUILDINGS 24

#define GORI_GORILLA_W 12
#define GORI_GORILLA_H 16

#define GORI_HUD_COLOR 15
#define GORI_SKY_COLOR 148
#define GORI_SKY_BAND_COLOR 150
#define GORI_SUN_COLOR 70
#define GORI_BUILDING_WINDOW 15
#define GORI_BUILDING_SHADOW 1
#define GORI_GORILLA_PLAYER_BASE 18
#define GORI_GORILLA_PLAYER_DOT 15
#define GORI_GORILLA_CPU_COLOR 34
#define GORI_BANANA_COLOR 53
#define GORI_EXPLOSION_COLOR_0 18
#define GORI_EXPLOSION_COLOR_1 68
#define GORI_METER_BG 1
#define GORI_METER_BORDER 15
#define GORI_METER_FILL 14
#define GORI_METER_W 88
#define GORI_METER_H 7

#define GORI_MAX_ANGLE 90
#define GORI_MAX_POWER 90

#define GORI_EXPLOSION_TICKS 10
#define GORI_FINISH_DELAY 24

typedef enum {
    GORI_STATE_INIT = 0,
    GORI_STATE_PLAYER_AIM,
    GORI_STATE_CPU_AIM,
    GORI_STATE_BANANA,
    GORI_STATE_EXPLOSION
} GoriState;

typedef enum {
    GORI_AFTER_NONE = 0,
    GORI_AFTER_TURN,
    GORI_AFTER_FINISH
} GoriAfter;

typedef struct {
    int x;
    int w;
    int h;
    unsigned char color;
} GoriBuilding;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} GoriRect;

typedef struct {
    float x;
    float y;
    float prev_x;
    float prev_y;
    float vx;
    float vy;
    int active;
} GoriBanana;

typedef struct {
    int wind_min;
    int wind_max;
    int turn_limit;
    int cpu_error;
    int cpu_adjust;
    int cpu_think_ticks;
    float cpu_power_scale;
    float gravity;
    float power_scale;
    float wind_scale;
    int substeps;
    int miss_penalty;
} GoriParams;

static GameSettings g_settings;
static GoriParams g_params;

static int gori_point_in_rect(int x, int y, const GoriRect *r);

static GoriBuilding g_buildings[GORI_MAX_BUILDINGS];
static int g_building_count = 0;
static int g_roof_y[VIDEO_WIDTH];

static GoriRect g_player_gorilla;
static GoriRect g_cpu_gorilla;

static GoriBanana g_banana;

static int g_state = GORI_STATE_INIT;
static int g_finished = 0;
static int g_did_win = 0;
static uint64_t g_score = 0;
static char g_end_detail[32] = "";
static int g_use_keyboard = 1;
static int g_sound_enabled = 0;
static int g_player_turns_left = 0;
static int g_current_shooter = 0;
static int g_finish_timer = 0;
static int g_explosion_timer = 0;
static int g_explosion_x = 0;
static int g_explosion_y = 0;
static GoriAfter g_explosion_after = GORI_AFTER_NONE;

static int g_player_angle = 45;
static int g_player_power = 50;
static int g_angle_hold = 0;
static int g_power_hold = 0;
static int g_angle_dir_prev = 0;
static int g_power_dir_prev = 0;
static int g_fire_held = 0;

static int g_wind = 0;

static int g_cpu_angle = 45;
static int g_cpu_power = 50;
static int g_cpu_guess_angle = 45;
static int g_cpu_guess_power = 50;
static int g_cpu_last_hit_roof = 0;
static int g_cpu_last_flew_past = 0;
static int g_cpu_has_guess = 0;
static int g_cpu_think = 0;
static int g_last_miss_x = 0;

static const SoundNote gori_throw_sound[] = {
    { 620, 18 },
    { 760, 22 }
};

static const SoundNote gori_explosion_sound[] = {
    { 240, 60 },
    { 180, 80 }
};

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static int rand_range(int min_value, int max_value)
{
    if (max_value <= min_value) {
        return min_value;
    }
    return min_value + (rand() % (max_value - min_value + 1));
}

static void gori_draw_circle(int cx, int cy, int r, unsigned char color)
{
    int y;

    for (y = -r; y <= r; ++y) {
        int yy = y * y;
        int x;
        for (x = -r; x <= r; ++x) {
            if ((x * x + yy) <= (r * r)) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < VIDEO_WIDTH && py >= 0 && py < VIDEO_HEIGHT) {
                    v_putpixel(px, py, color);
                }
            }
        }
    }
}

static void gori_draw_line(int x0, int y0, int x1, int y1, unsigned char color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        if (x0 >= 0 && x0 < VIDEO_WIDTH && y0 >= 0 && y0 < VIDEO_HEIGHT) {
            v_putpixel(x0, y0, color);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        if ((2 * err) >= dy) {
            err += dy;
            x0 += sx;
        }
        if ((2 * err) <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void gori_select_params(unsigned char difficulty, float speed_multiplier, GoriParams *params)
{
    if (!params) {
        return;
    }

    switch (difficulty) {
    case DIFFICULTY_EASY:
        params->wind_min = -2;
        params->wind_max = 2;
        params->turn_limit = 10;
        params->cpu_error = 10;
        params->cpu_adjust = 3;
        params->cpu_think_ticks = 10;
        params->cpu_power_scale = 0.45f;
        params->gravity = 0.25f;
        params->power_scale = 0.15f;
        params->wind_scale = 0.018f;
        params->substeps = 3;
        params->miss_penalty = 25;
        break;
    case DIFFICULTY_HARD:
        params->wind_min = -6;
        params->wind_max = 6;
        params->turn_limit = 6;
        params->cpu_error = 4;
        params->cpu_adjust = 8;
        params->cpu_think_ticks = 4;
        params->cpu_power_scale = 0.48f;
        params->gravity = 0.26f;
        params->power_scale = 0.16f;
        params->wind_scale = 0.025f;
        params->substeps = 4;
        params->miss_penalty = 40;
        break;
    case DIFFICULTY_NORMAL:
    default:
        params->wind_min = -4;
        params->wind_max = 4;
        params->turn_limit = 8;
        params->cpu_error = 6;
        params->cpu_adjust = 5;
        params->cpu_think_ticks = 7;
        params->cpu_power_scale = 0.46f;
        params->gravity = 0.25f;
        params->power_scale = 0.155f;
        params->wind_scale = 0.022f;
        params->substeps = 3;
        params->miss_penalty = 30;
        break;
    }

    if (speed_multiplier > 1.01f) {
        params->gravity *= speed_multiplier;
        params->power_scale *= speed_multiplier;
        params->wind_scale *= speed_multiplier;
        params->substeps = clamp_int((int)((float)params->substeps * speed_multiplier + 0.5f), 2, 6);
    }
}

static void gori_generate_city(void)
{
    static const unsigned char colors[] = {6, 7, 8, 9, 10, 11, 12, 13};
    int x = 0;
    int i;

    g_building_count = 0;

    while (x < VIDEO_WIDTH && g_building_count < GORI_MAX_BUILDINGS) {
        int w = rand_range(GORI_MIN_BUILDING_W, GORI_MAX_BUILDING_W);
        int h = rand_range(GORI_MIN_BUILDING_H, GORI_MAX_BUILDING_H);
        unsigned char color = colors[rand() % (int)(sizeof(colors) / sizeof(colors[0]))];
        GoriBuilding *b = &g_buildings[g_building_count];

        if (x + w > VIDEO_WIDTH) {
            w = VIDEO_WIDTH - x;
        }

        b->x = x;
        b->w = w;
        b->h = h;
        b->color = color;

        for (i = x; i < x + w && i < VIDEO_WIDTH; ++i) {
            g_roof_y[i] = VIDEO_HEIGHT - h;
        }

        x += w;
        g_building_count++;
    }

    for (i = x; i < VIDEO_WIDTH; ++i) {
        g_roof_y[i] = VIDEO_HEIGHT;
    }
}

static int gori_pick_building(int min_x, int max_x)
{
    int candidates[GORI_MAX_BUILDINGS];
    int count = 0;
    int i;

    for (i = 0; i < g_building_count; ++i) {
        int center = g_buildings[i].x + g_buildings[i].w / 2;
        if (center >= min_x && center <= max_x) {
            candidates[count++] = i;
        }
    }

    if (count == 0) {
        return -1;
    }

    return candidates[rand() % count];
}

static void gori_place_gorillas(void)
{
    int left_index = gori_pick_building(0, VIDEO_WIDTH / 3);
    int right_index = gori_pick_building((VIDEO_WIDTH * 2) / 3, VIDEO_WIDTH - 1);

    if (left_index < 0) {
        left_index = 0;
    }
    if (right_index < 0) {
        right_index = g_building_count > 0 ? g_building_count - 1 : 0;
    }

    {
        GoriBuilding *b = &g_buildings[left_index];
        int cx = b->x + b->w / 2;
        int roof = g_roof_y[clamp_int(cx, 0, VIDEO_WIDTH - 1)];
        g_player_gorilla.w = GORI_GORILLA_W;
        g_player_gorilla.h = GORI_GORILLA_H;
        g_player_gorilla.x = clamp_int(cx - (GORI_GORILLA_W / 2), 0, VIDEO_WIDTH - GORI_GORILLA_W);
        g_player_gorilla.y = clamp_int(roof - GORI_GORILLA_H, 0, VIDEO_HEIGHT - GORI_GORILLA_H);
    }

    {
        GoriBuilding *b = &g_buildings[right_index];
        int cx = b->x + b->w / 2;
        int roof = g_roof_y[clamp_int(cx, 0, VIDEO_WIDTH - 1)];
        g_cpu_gorilla.w = GORI_GORILLA_W;
        g_cpu_gorilla.h = GORI_GORILLA_H;
        g_cpu_gorilla.x = clamp_int(cx - (GORI_GORILLA_W / 2), 0, VIDEO_WIDTH - GORI_GORILLA_W);
        g_cpu_gorilla.y = clamp_int(roof - GORI_GORILLA_H, 0, VIDEO_HEIGHT - GORI_GORILLA_H);
    }
}

static void gori_reset_round(void)
{
    gori_generate_city();
    gori_place_gorillas();

    g_wind = rand_range(g_params.wind_min, g_params.wind_max);

    g_player_turns_left = g_params.turn_limit;
    g_current_shooter = 0;

    g_player_angle = 45;
    g_player_power = 55;
    g_angle_hold = 0;
    g_power_hold = 0;
    g_angle_dir_prev = 0;
    g_power_dir_prev = 0;
    g_fire_held = 0;

    g_cpu_angle = 45;
    g_cpu_power = 55;
    g_cpu_guess_angle = 45;
    g_cpu_guess_power = 55;
    g_cpu_has_guess = 0;
    g_cpu_think = g_params.cpu_think_ticks;

    g_banana.active = 0;

    g_explosion_timer = 0;
    g_explosion_after = GORI_AFTER_NONE;

    g_score = 0;
    g_finished = 0;
    g_did_win = 0;
    g_finish_timer = 0;
    g_end_detail[0] = '\0';

    g_state = GORI_STATE_PLAYER_AIM;
}

static void gori_draw_buildings(void)
{
    int i;

    for (i = 0; i < g_building_count; ++i) {
        GoriBuilding *b = &g_buildings[i];
        int roof = VIDEO_HEIGHT - b->h;
        int x;

        v_fill_rect(b->x, roof, b->w, b->h, b->color);

        for (x = b->x + 2; x < b->x + b->w - 2; x += 6) {
            int y;
            for (y = roof + 4; y < VIDEO_HEIGHT - 6; y += 6) {
                v_fill_rect(x, y, 2, 2, GORI_BUILDING_WINDOW);
            }
        }

        v_fill_rect(b->x, roof, 2, b->h, GORI_BUILDING_SHADOW);
    }
}

static void gori_draw_gorilla(const GoriRect *g, int is_player)
{
    int body_x = g->x + 2;
    int body_y = g->y + 6;
    int body_w = g->w - 4;
    int body_h = g->h - 6;
    int head_x = g->x + g->w / 2;
    int head_y = g->y + 4;

    if (is_player) {
        v_draw_dotted_rect(body_x, body_y, body_w, body_h, GORI_GORILLA_PLAYER_BASE, GORI_GORILLA_PLAYER_DOT, 0);
        gori_draw_circle(head_x, head_y, 3, GORI_GORILLA_PLAYER_BASE);
        if (head_x - 1 >= 0 && head_x - 1 < VIDEO_WIDTH && head_y - 1 >= 0 && head_y - 1 < VIDEO_HEIGHT) {
            v_putpixel(head_x - 1, head_y - 1, GORI_GORILLA_PLAYER_DOT);
        }
        if (head_x + 1 >= 0 && head_x + 1 < VIDEO_WIDTH && head_y + 1 >= 0 && head_y + 1 < VIDEO_HEIGHT) {
            v_putpixel(head_x + 1, head_y + 1, GORI_GORILLA_PLAYER_DOT);
        }
        // Densifica el jersey con puntitos blancos
    } else {
        v_fill_rect(body_x, body_y, body_w, body_h, GORI_GORILLA_CPU_COLOR);
        gori_draw_circle(head_x, head_y, 3, GORI_GORILLA_CPU_COLOR);
    }

    gori_draw_line(body_x, body_y + 2, body_x - 3, body_y - 3,
                   is_player ? GORI_GORILLA_PLAYER_BASE : GORI_GORILLA_CPU_COLOR);
    gori_draw_line(body_x + body_w - 1, body_y + 2, body_x + body_w + 2, body_y - 3,
                   is_player ? GORI_GORILLA_PLAYER_BASE : GORI_GORILLA_CPU_COLOR);
}

static void gori_draw_meter(int x, int y, int w, int h, int value, int max_value)
{
    int inner_w;
    int inner_h;
    int fill;
    int marker_x;

    if (w < 3 || h < 3 || max_value <= 0) {
        return;
    }

    value = clamp_int(value, 0, max_value);

    v_fill_rect(x, y, w, h, GORI_METER_BG);
    v_fill_rect(x, y, w, 1, GORI_METER_BORDER);
    v_fill_rect(x, y + h - 1, w, 1, GORI_METER_BORDER);
    v_fill_rect(x, y, 1, h, GORI_METER_BORDER);
    v_fill_rect(x + w - 1, y, 1, h, GORI_METER_BORDER);

    inner_w = w - 2;
    inner_h = h - 2;
    fill = (value * inner_w) / max_value;
    if (fill > 0) {
        v_fill_rect(x + 1, y + 1, fill, inner_h, GORI_METER_FILL);
    }

    marker_x = x + 1 + (value * (inner_w - 1)) / max_value;
    v_fill_rect(marker_x, y, 1, h, GORI_METER_FILL);
}

static void gori_draw_hud(void)
{
    char text[32];
    int meter_x = 8;
    int meter_y = 42;

    snprintf(text, sizeof(text), "ANGULO: %03d", g_player_angle);
    v_puts(8, 8, text, GORI_HUD_COLOR);

    snprintf(text, sizeof(text), "POTENCIA: %03d", g_player_power);
    v_puts(8, 18, text, GORI_HUD_COLOR);

    if (g_wind < 0) {
        snprintf(text, sizeof(text), "Wind: <-%d", -g_wind);
    } else {
        snprintf(text, sizeof(text), "Wind: +%d", g_wind);
    }
    v_puts(8, 28, text, GORI_HUD_COLOR);

    v_puts(meter_x, meter_y, "ANG", GORI_HUD_COLOR);
    gori_draw_meter(meter_x + 32, meter_y, GORI_METER_W, GORI_METER_H, g_player_angle, GORI_MAX_ANGLE);

    v_puts(meter_x, meter_y + 12, "POT", GORI_HUD_COLOR);
    gori_draw_meter(meter_x + 32, meter_y + 12, GORI_METER_W, GORI_METER_H, g_player_power, GORI_MAX_POWER);
}

static void gori_draw_banana(float alpha)
{
    if (!g_banana.active) {
        return;
    }

    {
        float x = lerpf(g_banana.prev_x, g_banana.x, alpha);
        float y = lerpf(g_banana.prev_y, g_banana.y, alpha);
        int px = (int)(x + 0.5f);
        int py = (int)(y + 0.5f);

        int dx, dy, i;

        // Borde negro alrededor de los 3 píxeles
        for (i = -1; i <= 1; ++i) {
            int bx = px + i;
            int by = py;

            for (dy = -1; dy <= 1; ++dy) {
                for (dx = -1; dx <= 1; ++dx) {
                    int x2 = bx + dx;
                    int y2 = by + dy;

                    // OJO: no tapar el relleno amarillo
                    if (dx == 0 && dy == 0) {
                        continue;
                    }

                    if (x2 >= 0 && x2 < VIDEO_WIDTH && y2 >= 0 && y2 < VIDEO_HEIGHT) {
                        v_putpixel(x2, y2, 2);
                    }
                }
            }
        }

        // Núcleo de la banana
        if (px >= 0 && px < VIDEO_WIDTH && py >= 0 && py < VIDEO_HEIGHT) {
            v_putpixel(px, py, GORI_BANANA_COLOR);
        }
        if (px - 1 >= 0 && px - 1 < VIDEO_WIDTH && py >= 0 && py < VIDEO_HEIGHT) {
            v_putpixel(px - 1, py, GORI_BANANA_COLOR);
        }
        if (px + 1 >= 0 && px + 1 < VIDEO_WIDTH && py >= 0 && py < VIDEO_HEIGHT) {
            v_putpixel(px + 1, py, GORI_BANANA_COLOR);
        }
    }
}

static void gori_draw_explosion(void)
{
    if (g_explosion_timer <= 0) {
        return;
    }

    {
        int stage = GORI_EXPLOSION_TICKS - g_explosion_timer;
        int radius = 2 + stage;
        int inner = radius / 2;

        gori_draw_circle(g_explosion_x, g_explosion_y, radius, GORI_EXPLOSION_COLOR_0);
        gori_draw_circle(g_explosion_x, g_explosion_y, inner, GORI_EXPLOSION_COLOR_1);
    }
}

static void gori_start_explosion(int x, int y, GoriAfter after)
{
    g_explosion_x = x;
    g_explosion_y = y;
    g_explosion_timer = GORI_EXPLOSION_TICKS;
    g_explosion_after = after;
    g_state = GORI_STATE_EXPLOSION;

    if (g_sound_enabled) {
        sound_play_melody(gori_explosion_sound,
                          (int)(sizeof(gori_explosion_sound) / sizeof(gori_explosion_sound[0])));
    }
}

static void gori_finish(int did_win, const char *detail)
{
    if (g_finish_timer > 0) {
        return;
    }

    g_did_win = did_win ? 1 : 0;

    if (detail && detail[0] != '\0') {
        snprintf(g_end_detail, sizeof(g_end_detail), "%s", detail);
    } else {
        g_end_detail[0] = '\0';
    }

    if (g_did_win) {
        uint64_t bonus = (uint64_t)g_player_turns_left * 75u;
        g_score += 1000u + bonus;
    }

    g_finish_timer = GORI_FINISH_DELAY;
}

static void gori_handle_turn_end(int miss_x)
{
    if (g_current_shooter == 0) {
        if (g_score >= (uint64_t)g_params.miss_penalty) {
            g_score -= (uint64_t)g_params.miss_penalty;
        } else {
            g_score = 0;
        }
        if (g_player_turns_left <= 0) {
            gori_finish(0, "SIN TURNOS");
            return;
        }
        g_current_shooter = 1;
        g_cpu_think = g_params.cpu_think_ticks;
        g_state = GORI_STATE_CPU_AIM;
    } else {
        g_last_miss_x = miss_x;
        g_current_shooter = 0;
        g_state = GORI_STATE_PLAYER_AIM;
    }
}

static void gori_adjust_cpu_guess(void)
{
    int target_x = g_player_gorilla.x + g_player_gorilla.w / 2;

    if (!g_cpu_has_guess) {
        return;
    }

    if (g_last_miss_x < 0) {
        g_cpu_guess_power -= g_params.cpu_adjust;
    } else if (g_last_miss_x >= VIDEO_WIDTH) {
        g_cpu_guess_power += g_params.cpu_adjust;
    } else if (g_last_miss_x < target_x) {
        g_cpu_guess_power -= g_params.cpu_adjust;
    } else if (g_last_miss_x > target_x) {
        g_cpu_guess_power += g_params.cpu_adjust;
    }

    // Ajuste humano según el fallo
    if (g_cpu_last_hit_roof) {
        // Chocó pronto: necesita más arco
        g_cpu_guess_angle += (g_settings.difficulty == DIFFICULTY_EASY) ? 3 : 2;

        // Reduce potencia para no repetir el choque
        g_cpu_guess_power -= 2;

        g_cpu_last_hit_roof = 0;
    }

    if (g_cpu_last_flew_past) {
        // Voló demasiado: baja arco o potencia
        g_cpu_guess_angle -= 1;
        g_cpu_guess_power -= 2;

        g_cpu_last_flew_past = 0;
    }

    // OJO: clamps de seguridad
    g_cpu_guess_angle = clamp_int(g_cpu_guess_angle, 10, GORI_MAX_ANGLE);
    g_cpu_guess_power = clamp_int(g_cpu_guess_power, 10, GORI_MAX_POWER);

    g_cpu_guess_power = clamp_int(g_cpu_guess_power, 15, GORI_MAX_POWER);
}

static void gori_start_shot(int shooter, int angle, int power)
{
    float radians = (float)angle * 3.1415926f / 180.0f;
    float speed = (float)power * g_params.power_scale;
    float vx = speed * (float)cos(radians);
    float vy = -speed * (float)sin(radians);
    GoriRect *g = (shooter == 0) ? &g_player_gorilla : &g_cpu_gorilla;
    float start_x = (float)(g->x + g->w / 2);
    float start_y = (float)(g->y + 2);

    if (shooter != 0) {
        vx = -vx;
    }

    if (shooter == 1) {
        g_cpu_last_hit_roof = 0;
        g_cpu_last_flew_past = 0;
    }

    g_banana.x = start_x;
    g_banana.y = start_y;
    g_banana.prev_x = start_x;
    g_banana.prev_y = start_y;
    g_banana.vx = vx;
    g_banana.vy = vy;
    g_banana.active = 1;

    g_state = GORI_STATE_BANANA;

    if (g_sound_enabled) {
        sound_play_melody(gori_throw_sound,
                          (int)(sizeof(gori_throw_sound) / sizeof(gori_throw_sound[0])));
    }
}

static void gori_handle_player_input(void)
{
    int angle_dir = 0;
    int power_dir = 0;
    int fire_pressed = 0;

    if (g_use_keyboard) {
        if (kb_down(SC_LEFT)) {
            angle_dir = -1;
        } else if (kb_down(SC_RIGHT)) {
            angle_dir = 1;
        }

        if (kb_down(SC_UP)) {
            power_dir = 1;
        } else if (kb_down(SC_DOWN)) {
            power_dir = -1;
        }

        fire_pressed = Input_Pressed(SC_SPACE);
    } else {
        int dir_x = 0;
        int dir_y = 0;
        unsigned char buttons = 0;
        if (in_joystick_direction(&dir_x, &dir_y, &buttons)) {
            if (dir_x < 0) {
                angle_dir = -1;
            } else if (dir_x > 0) {
                angle_dir = 1;
            }

            if (dir_y < 0) {
                power_dir = 1;
            } else if (dir_y > 0) {
                power_dir = -1;
            }

            if (buttons & JOY_BUTTON_ENTER) {
                fire_pressed = !g_fire_held;
                g_fire_held = 1;
            } else {
                g_fire_held = 0;
            }
        }
    }

    if (angle_dir != 0) {
        if (angle_dir != g_angle_dir_prev) {
            g_angle_hold = 0;
        }
        g_angle_hold++;
    } else {
        g_angle_hold = 0;
    }
    g_angle_dir_prev = angle_dir;

    if (power_dir != 0) {
        if (power_dir != g_power_dir_prev) {
            g_power_hold = 0;
        }
        g_power_hold++;
    } else {
        g_power_hold = 0;
    }
    g_power_dir_prev = power_dir;

    // Autorepetición con aceleración en ángulo
    if (angle_dir != 0) {
        int delta = 0;

        if (g_angle_hold == 1) {
            // Toque siempre 1
            delta = 1;
        } else if (g_angle_hold > 8) {
            // Mantener: empieza la repetición tras un retardo
            int step = 1;
            int rate = 2; // Cada 2 frames

            if (g_angle_hold > 30) {
                step = 4;
                rate = 1; // Aceleración máxima
            } else if (g_angle_hold > 18) {
                step = 2;
                rate = 2;
            }

            // Repite a intervalos
            if (((g_angle_hold - 9) % rate) == 0) {
                delta = step;
            }
        }

        if (delta != 0) {
            g_player_angle = clamp_int(g_player_angle + (angle_dir * delta), 0, GORI_MAX_ANGLE);
        }
    }

    // Autorepetición en potencia
    if (power_dir != 0) {
        int delta = 0;

        if (g_power_hold == 1) {
            delta = 1;
        } else if (g_power_hold > 8) {
            int step = 1;
            int rate = 2;

            if (g_power_hold > 30) {
                step = 4;
                rate = 1;
            } else if (g_power_hold > 18) {
                step = 2;
                rate = 2;
            }

            if (((g_power_hold - 9) % rate) == 0) {
                delta = step;
            }
        }

        if (delta != 0) {
            g_player_power = clamp_int(g_player_power + (power_dir * delta), 0, GORI_MAX_POWER);
        }
    }

    if (fire_pressed && g_player_turns_left > 0) {
        g_player_turns_left--;
        g_current_shooter = 0;
        gori_start_shot(0, g_player_angle, g_player_power);
    }
}

static long gori_cpu_eval_shot(int angle, int power, int wind_used, int *out_hit)
{
    // OJO: si impacta, *out_hit=1 y devuelve 0
    float radians = (float)angle * 3.1415926f / 180.0f;
    float speed = (float)power * g_params.power_scale;
    float vx = speed * (float)cos(radians);
    float vy = -speed * (float)sin(radians);

    // CPU dispara hacia la izquierda
    vx = -vx;

    // Punto de salida igual que gori_start_shot
    {
        float x = (float)(g_cpu_gorilla.x + g_cpu_gorilla.w / 2);
        float y = (float)(g_cpu_gorilla.y + 2);

        float wind_accel = (float)wind_used * g_params.wind_scale;
        int steps = clamp_int(g_params.substeps, 1, 6);
        float step = 1.0f / (float)steps;

        int tx = g_player_gorilla.x + g_player_gorilla.w / 2;
        int ty = g_player_gorilla.y + g_player_gorilla.h / 2;

        long best = LONG_MAX;
        int i, t;

        *out_hit = 0;

        // OJO: límite de pasos para no colgarse
        for (t = 0; t < 260; ++t) {
            for (i = 0; i < steps; ++i) {
                int bx, by;
                long dx, dy;
                long d2;

                vy += g_params.gravity * step;
                vx += wind_accel * step;
                x += vx * step;
                y += vy * step;

                bx = (int)(x + 0.5f);
                by = (int)(y + 0.5f);

                if (bx < 0 || bx >= VIDEO_WIDTH || by >= VIDEO_HEIGHT) {
                    return best;
                }

                if (gori_point_in_rect(bx, by, &g_player_gorilla)) {
                    *out_hit = 1;
                    return 0;
                }

                if (by >= g_roof_y[bx]) {
                    return best;
                }

                dx = (long)bx - (long)tx;
                dy = (long)by - (long)ty;
                d2 = dx * dx + dy * dy;
                if (d2 < best) {
                    best = d2;
                }
            }
        }

        return best;
    }
}

typedef enum {
    CPU_SIM_OFFSCREEN = 0,
    CPU_SIM_HIT_ROOF  = 1,
    CPU_SIM_HIT_TARGET = 2
} CpuSimResult;

static CpuSimResult gori_cpu_simulate_shot(int shooter, int angle, int power, int *out_last_x)
{
    float radians = (float)angle * 3.1415926f / 180.0f;
    float speed = (float)power * g_params.power_scale;
    float vx = speed * (float)cos(radians);
    float vy = -speed * (float)sin(radians);

    const GoriRect *g = (shooter == 0) ? &g_player_gorilla : &g_cpu_gorilla;
    const GoriRect *target = (shooter == 0) ? &g_cpu_gorilla : &g_player_gorilla;

    float x = (float)(g->x + g->w / 2);
    float y = (float)(g->y + 2);

    float wind_accel = (float)g_wind * g_params.wind_scale;
    int steps = clamp_int(g_params.substeps, 1, 6);
    float step = 1.0f / (float)steps;

    int i;
    int it;
    int max_iters = 1200; // OJO: límite para cruzar pantalla

    if (shooter != 0) {
        vx = -vx;
    }

    for (it = 0; it < max_iters; ++it) {
        for (i = 0; i < steps; ++i) {
            int bx;
            int by;

            vy += g_params.gravity * step;
            vx += wind_accel * step;
            x += vx * step;
            y += vy * step;

            bx = (int)(x + 0.5f);
            by = (int)(y + 0.5f);

            if (out_last_x) {
                *out_last_x = bx;
            }

            if (bx < 0 || bx >= VIDEO_WIDTH || by >= VIDEO_HEIGHT) {
                return CPU_SIM_OFFSCREEN;
            }

            if (gori_point_in_rect(bx, by, target)) {
                return CPU_SIM_HIT_TARGET;
            }

            if (by >= g_roof_y[bx]) {
                return CPU_SIM_HIT_ROOF;
            }
        }
    }

    return CPU_SIM_OFFSCREEN;
}

static void gori_cpu_pick_shot(int *out_angle, int *out_power)
{
    int target_x = g_player_gorilla.x + g_player_gorilla.w / 2;

    // Calidad de búsqueda según dificultad
    int a_min = 15, a_max = GORI_MAX_ANGLE;
    int p_min = 10, p_max = GORI_MAX_POWER;

    int a_step = 3;
    int p_step = 3;
    int aim_jitter = 0;

    if (g_settings.difficulty == DIFFICULTY_EASY) {
        a_step = 5; p_step = 5; aim_jitter = 8;
    } else if (g_settings.difficulty == DIFFICULTY_HARD) {
        a_step = 2; p_step = 2; aim_jitter = 3;
    } else {
        a_step = 3; p_step = 3; aim_jitter = 5;
    }

    // Busca mejor tiro
    {
        int best_angle = 45;
        int best_power = 50;
        int best_cost = INT_MAX;

        int angle;
        for (angle = a_min; angle <= a_max; angle += a_step) {
            int power;
            for (power = p_min; power <= p_max; power += p_step) {
                int last_x = 0;
                CpuSimResult r = gori_cpu_simulate_shot(1, angle, power, &last_x);

                if (r == CPU_SIM_HIT_TARGET) {
                    best_angle = angle;
                    best_power = power;
                    best_cost = 0;
                    goto done_search;
                } else {
                    int dx = target_x - last_x;
                    int cost = dx;
                    if (cost < 0) cost = -cost;

                    // Penaliza estamparse para buscar tiros altos
                    if (r == CPU_SIM_HIT_ROOF) {
                        cost += 40;
                    }

                    if (cost < best_cost) {
                        best_cost = cost;
                        best_angle = angle;
                        best_power = power;
                    }
                }
            }
        }

done_search:
        // Mete error humano sin romper el tiro
        best_angle += rand_range(-aim_jitter, aim_jitter);
        best_power += rand_range(-aim_jitter, aim_jitter);

        *out_angle = clamp_int(best_angle, 10, GORI_MAX_ANGLE);
        *out_power = clamp_int(best_power, 10, GORI_MAX_POWER);
    }
}

static void gori_handle_cpu_turn(void)
{
    if (g_cpu_think > 0) {
        g_cpu_think--;
        return;
    }

    // CPU elige tiro probando la física real
    gori_cpu_pick_shot(&g_cpu_angle, &g_cpu_power);

    g_current_shooter = 1;
    gori_start_shot(1, g_cpu_angle, g_cpu_power);
}

static int gori_point_in_rect(int x, int y, const GoriRect *r)
{
    return x >= r->x && x < (r->x + r->w) && y >= r->y && y < (r->y + r->h);
}

static void gori_update_banana(void)
{
    float wind_accel = (float)g_wind * g_params.wind_scale;
    int steps = clamp_int(g_params.substeps, 1, 6);
    float step = 1.0f / (float)steps;
    int i;

    for (i = 0; i < steps; ++i) {
        int bx;
        int by;
        const GoriRect *target = (g_current_shooter == 0) ? &g_cpu_gorilla : &g_player_gorilla;

        g_banana.vy += g_params.gravity * step;
        g_banana.vx += wind_accel * step;
        g_banana.x += g_banana.vx * step;
        g_banana.y += g_banana.vy * step;

        bx = (int)(g_banana.x + 0.5f);
        by = (int)(g_banana.y + 0.5f);

        if (bx < 0 || bx >= VIDEO_WIDTH || by >= VIDEO_HEIGHT) {
            g_banana.active = 0;
            if (g_current_shooter == 1) {
                // Marca que la CPU se pasó
                g_cpu_last_hit_roof = 0;
                g_cpu_last_flew_past = 1;
            }
            gori_handle_turn_end(bx);
            return;
        }

        if (gori_point_in_rect(bx, by, target)) {
            g_banana.active = 0;
            gori_start_explosion(bx, by, GORI_AFTER_FINISH);
            if (g_current_shooter == 0) {
                gori_finish(1, "IMPACTO");
            } else {
                gori_finish(0, "DERROTA");
            }
            return;
        }

        if (by >= g_roof_y[bx]) {
            g_banana.active = 0;
            gori_start_explosion(bx, by, GORI_AFTER_TURN);
            if (g_current_shooter == 1) {
                // Marca choque con tejado
                g_cpu_last_hit_roof = 1;
                g_cpu_last_flew_past = 0;
            }
            return;
        }
    }
}

void Gori_Init(const GameSettings *settings)
{
    if (settings) {
        g_settings = *settings;
    } else {
        memset(&g_settings, 0, sizeof(g_settings));
    }

    g_use_keyboard = 1;
    if (g_settings.input_mode == INPUT_JOYSTICK) {
        if (in_joystick_available()) {
            g_use_keyboard = 0;
        }
    }

    gori_select_params(g_settings.difficulty, g_settings.speed_multiplier, &g_params);
    g_state = GORI_STATE_INIT;
    g_finish_timer = 0;
    g_explosion_timer = 0;
    g_explosion_after = GORI_AFTER_NONE;
    g_banana.active = 0;
    g_sound_enabled = g_settings.sound_enabled ? 1 : 0;

    gori_reset_round();

    Gori_StorePreviousState();

    while (in_keyhit()) {
        in_poll();
    }
}

void Gori_StorePreviousState(void)
{
    g_banana.prev_x = g_banana.x;
    g_banana.prev_y = g_banana.y;
}

void Gori_Update(void)
{
    sound_update();
    if (g_finished) {
        return;
    }

    if (g_finish_timer > 0) {
        g_finish_timer--;
        if (g_finish_timer <= 0) {
            g_finished = 1;
        }
        return;
    }

    switch (g_state) {
    case GORI_STATE_INIT:
        gori_reset_round();
        break;
    case GORI_STATE_PLAYER_AIM:
        if (g_player_turns_left <= 0) {
            gori_finish(0, "SIN TURNOS");
            break;
        }
        gori_handle_player_input();
        break;
    case GORI_STATE_CPU_AIM:
        gori_handle_cpu_turn();
        break;
    case GORI_STATE_BANANA:
        gori_update_banana();
        break;
    case GORI_STATE_EXPLOSION:
        if (g_explosion_timer > 0) {
            g_explosion_timer--;
        }
        if (g_explosion_timer <= 0) {
            if (g_explosion_after == GORI_AFTER_TURN) {
                g_explosion_after = GORI_AFTER_NONE;
                gori_handle_turn_end(g_explosion_x);
            } else if (g_explosion_after == GORI_AFTER_FINISH) {
                g_explosion_after = GORI_AFTER_NONE;
            }
        }
        break;
    default:
        break;
    }
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

void Gori_DrawInterpolated(float alpha)
{
    char hud[64];
    char bottom[32];

    // Fondo base
    v_fill_rect(0, 0, VIDEO_WIDTH, VIDEO_HEIGHT / 2, GORI_SKY_BAND_COLOR);
    v_fill_rect(0, VIDEO_HEIGHT / 2, VIDEO_WIDTH, VIDEO_HEIGHT / 2, GORI_SKY_COLOR);

    // Franja HUD superior
    v_fill_rect(0, 0, VIDEO_WIDTH, 8, 0);

    gori_draw_circle(40, 36, 8, GORI_SUN_COLOR);
    gori_draw_buildings();
    gori_draw_gorilla(&g_player_gorilla, 1);
    gori_draw_gorilla(&g_cpu_gorilla, 0);
    gori_draw_banana(alpha);
    gori_draw_explosion();

    if (g_state == GORI_STATE_PLAYER_AIM || g_state == GORI_STATE_BANANA || g_state == GORI_STATE_EXPLOSION) {
        gori_draw_hud();
    }

    // Texto HUD superior
    snprintf(hud, sizeof(hud), "D:%s x%.2f", difficulty_short(g_settings.difficulty), g_settings.speed_multiplier);
    v_puts(0, 0, "1991", 7);
    v_puts(VIDEO_WIDTH - (text_len(hud) * 8), 0, hud, 7);

    // HUD inferior con tiros restantes
    v_fill_rect(0, VIDEO_HEIGHT - 8, VIDEO_WIDTH, 8, 0);
    snprintf(bottom, sizeof(bottom), "PLATANOS %02d", g_player_turns_left);
    v_puts(VIDEO_WIDTH - (text_len(bottom) * 8), VIDEO_HEIGHT - 8, bottom, 15);

    v_present();
}


void Gori_End(void)
{
    // End no borra el resultado, solo prepara el texto final

    if (g_did_win) {
        char score_text[16];
        high_scores_format_score(score_text, sizeof(score_text), g_score);
        snprintf(g_end_detail, sizeof(g_end_detail), "PUNTOS %s", score_text);
    }
    // Limpieza mínima
    g_banana.active = 0;
}

int Gori_IsFinished(void)
{
    return g_finished;
}

int Gori_DidWin(void)
{
    return g_did_win;
}

const char *Gori_GetEndDetail(void)
{
    return g_end_detail;
}

uint64_t Gori_GetScore(void)
{
    return g_score;
}
