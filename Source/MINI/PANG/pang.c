#include "pang.h"

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
#include <malloc.h>

#define PANG_LEFT 8
#define PANG_RIGHT 312
#define PANG_TOP 16
#define PANG_FLOOR 184

#define PANG_PLAYER_W 32
#define PANG_PLAYER_H 48
#define PANG_ARROW_W 6
#define PANG_ARROW_H 6

#define PANG_BALL_XL 24
#define PANG_BALL_M 12
#define PANG_BALL_S 6

#define PANG_MAX_BALLS 16
#define PANG_TICKS_PER_SECOND 60
#define PANG_WALK_TICKS 60 // Ticks por frame al caminar

#define PANG_SKY_COLOR 148
#define PANG_SKY_BAND_COLOR 152
#define PANG_GRASS_COLOR 104
#define PANG_GRASS_SHADOW_COLOR 96
#define PANG_GRASS_DOT_COLOR 110
#define PANG_FENCE_COLOR 6
#define PANG_HORIZON_COLOR 15
#define PANG_LINE_COLOR 15
#define PANG_HUD_BG_COLOR 1

/* -------------------------------------------------------------------------
   PUNTUACIÓN BASE
   ------------------------------------------------------------------------- */
#define PANG_SCORE_HIT_XL 100
#define PANG_SCORE_HIT_M 200
#define PANG_SCORE_HIT_S 400

/* -------------------------------------------------------------------------
   BONUS DE TIEMPO
   ------------------------------------------------------------------------- */
#define PANG_TIME_BONUS_PER_SEC 25 // Puntos por segundo ahorrado
#define PANG_TIME_TARGET_EASY 70   // Segundos objetivo para bonus
#define PANG_TIME_TARGET_NORMAL 90
#define PANG_TIME_TARGET_HARD 110

/* -------------------------------------------------------------------------
   TIEMPO MÁXIMO POR DIFICULTAD
   ------------------------------------------------------------------------- */
#define PANG_TIME_LIMIT_EASY 120
#define PANG_TIME_LIMIT_NORMAL 150
#define PANG_TIME_LIMIT_HARD 180

typedef enum {
    PANG_SIZE_XL = 0,
    PANG_SIZE_M = 1,
    PANG_SIZE_S = 2
} PangBallSize;

typedef struct {
    float player_speed;
    float ball_speed_x;
    float ball_bounce_vy;
    float gravity;
    float arrow_speed;
    int initial_balls;
} PangParams;

typedef struct {
    unsigned short w;
    unsigned short h;
    unsigned long max_pixels;
    unsigned char far *pixels;
} PangSprite;

typedef struct {
    float x;
    float y;
    float x_prev;
    float y_prev;
    float vx;
    float vy;
    PangBallSize size;
    int active;
} PangBall;

static GameSettings g_settings;
static PangParams g_params;
static int g_finished = 0;
static int g_did_win = 0;
static int g_sound_enabled = 0;
static int g_use_keyboard = 1;

static PangSprite g_player1 = {0, 0, PANG_PLAYER_W * PANG_PLAYER_H, NULL};
static PangSprite g_player2 = {0, 0, PANG_PLAYER_W * PANG_PLAYER_H, NULL};
static PangSprite g_player3 = {0, 0, PANG_PLAYER_W * PANG_PLAYER_H, NULL};
static PangSprite g_ball_xl = {0, 0, PANG_BALL_XL * PANG_BALL_XL, NULL};
static PangSprite g_ball_m = {0, 0, PANG_BALL_M * PANG_BALL_M, NULL};
static PangSprite g_ball_s = {0, 0, PANG_BALL_S * PANG_BALL_S, NULL};
static PangSprite g_arrow = {0, 0, PANG_ARROW_W * PANG_ARROW_H, NULL};
static int g_sprites_loaded = 0;

static PangBall g_balls[PANG_MAX_BALLS];

static float g_player_x = 0.0f;
static float g_player_x_prev = 0.0f;
static float g_player_y = 0.0f;
static int g_player_dir = 1;
static int g_player_move_dir = 0;
static int g_walk_ticks = 0;
static int g_walk_frame = 0;

static int g_arrow_active = 0;
static float g_arrow_x = 0.0f;
static float g_arrow_y = 0.0f;
static float g_arrow_x_prev = 0.0f;
static float g_arrow_y_prev = 0.0f;
static int g_fire_held = 0;

static char g_end_detail[32] = "";
static uint64_t g_score = 0;
static uint32_t g_elapsed_ticks = 0;

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

static float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
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

static int pang_ball_size_px(PangBallSize size)
{
    switch (size) {
    case PANG_SIZE_XL:
        return PANG_BALL_XL;
    case PANG_SIZE_M:
        return PANG_BALL_M;
    case PANG_SIZE_S:
    default:
        return PANG_BALL_S;
    }
}

static uint32_t pang_score_for_hit(PangBallSize size)
{
    switch (size) {
    case PANG_SIZE_XL:
        return PANG_SCORE_HIT_XL;
    case PANG_SIZE_M:
        return PANG_SCORE_HIT_M;
    case PANG_SIZE_S:
    default:
        return PANG_SCORE_HIT_S;
    }
}

static uint32_t pang_time_target_seconds(unsigned char difficulty)
{
    switch (difficulty) {
    case DIFFICULTY_EASY:
        return PANG_TIME_TARGET_EASY;
    case DIFFICULTY_HARD:
        return PANG_TIME_TARGET_HARD;
    case DIFFICULTY_NORMAL:
    default:
        return PANG_TIME_TARGET_NORMAL;
    }
}

static uint32_t pang_time_limit_seconds(unsigned char difficulty)
{
    switch (difficulty) {
    case DIFFICULTY_EASY:
        return PANG_TIME_LIMIT_EASY;
    case DIFFICULTY_HARD:
        return PANG_TIME_LIMIT_HARD;
    case DIFFICULTY_NORMAL:
    default:
        return PANG_TIME_LIMIT_NORMAL;
    }
}

static float pang_ball_bounce_vy(PangBallSize size)
{
    switch (size) {
    case PANG_SIZE_XL:
        return g_params.ball_bounce_vy * 1.2f;
    case PANG_SIZE_M:
        return g_params.ball_bounce_vy * 1.1f;
    case PANG_SIZE_S:
    default:
        return g_params.ball_bounce_vy;
    }
}

static int pang_ball_margin(PangBallSize size)
{
    switch (size) {
    case PANG_SIZE_XL:
        return 4;
    case PANG_SIZE_M:
        return 2;
    case PANG_SIZE_S:
    default:
        return 1;
    }
}

static int pang_alloc_sprite_pixels(PangSprite *sprite)
{
    if (!sprite) {
        return 0;
    }
    if (!sprite->pixels) {
        sprite->pixels = (unsigned char far *)_fmalloc(sprite->max_pixels);
        if (!sprite->pixels) {
            return 0;
        }
    }
    return 1;
}

static int pang_load_sprite(const char *path, PangSprite *sprite)
{
    if (!sprite || !path) {
        return 0;
    }

    sprite->w = 0;
    sprite->h = 0;

    if (!pang_alloc_sprite_pixels(sprite)) {
        return 0;
    }

    return sprite_dat_load_auto(path, &sprite->w, &sprite->h, sprite->pixels, sprite->max_pixels);
}

static void pang_load_sprites(void)
{
    if (g_sprites_loaded) {
        return;
    }

    pang_load_sprite("SPRITES\\pang1.dat", &g_player1);
    pang_load_sprite("SPRITES\\pang2.dat", &g_player2);
    pang_load_sprite("SPRITES\\pang3.dat", &g_player3);
    pang_load_sprite("SPRITES\\ballxl.dat", &g_ball_xl);
    pang_load_sprite("SPRITES\\ballm.dat", &g_ball_m);
    pang_load_sprite("SPRITES\\balls.dat", &g_ball_s);
    pang_load_sprite("SPRITES\\arrow.dat", &g_arrow);

    g_sprites_loaded = 1;
}

static void pang_free_sprite(PangSprite *sprite)
{
    if (!sprite || !sprite->pixels) {
        return;
    }

    _ffree(sprite->pixels);
    sprite->pixels = NULL;
    sprite->w = 0;
    sprite->h = 0;
}

static void pang_free_sprites(void)
{
    if (!g_sprites_loaded) {
        return;
    }

    pang_free_sprite(&g_player1);
    pang_free_sprite(&g_player2);
    pang_free_sprite(&g_player3);
    pang_free_sprite(&g_ball_xl);
    pang_free_sprite(&g_ball_m);
    pang_free_sprite(&g_ball_s);
    pang_free_sprite(&g_arrow);

    g_sprites_loaded = 0;
}

static void pang_blit_sprite(int x, int y, const PangSprite *sprite)
{
    if (!sprite || sprite->w == 0 || sprite->h == 0) {
        return;
    }
    v_blit_sprite(x, y, sprite->w, sprite->h, sprite->pixels, 0);
}

static void pang_blit_sprite_flipped(int x, int y, const PangSprite *sprite, int flip_x)
{
    int sx;
    int sy;
    int w;
    int h;

    if (!sprite || sprite->w == 0 || sprite->h == 0) {
        return;
    }

    w = sprite->w;
    h = sprite->h;
    for (sy = 0; sy < h; ++sy) {
        int src_y = sy;
        for (sx = 0; sx < w; ++sx) {
            int src_x = flip_x ? (w - 1 - sx) : sx;
            unsigned char color = sprite->pixels[src_y * w + src_x];
            if (color != 0) {
                v_putpixel(x + sx, y + sy, color);
            }
        }
    }
}

static void pang_draw_background(void)
{
    int x;
    int y;
    int grass_start = PANG_FLOOR - 32;

    v_clear(PANG_SKY_COLOR);

    v_fill_rect(0, PANG_TOP, VIDEO_WIDTH, 24, PANG_SKY_BAND_COLOR);
    v_fill_rect(0, grass_start, VIDEO_WIDTH, VIDEO_HEIGHT - grass_start, PANG_GRASS_COLOR);
    v_fill_rect(0, grass_start + 20, VIDEO_WIDTH, VIDEO_HEIGHT - (grass_start + 20),
                PANG_GRASS_SHADOW_COLOR);
    v_fill_rect(0, grass_start - 2, VIDEO_WIDTH, 2, PANG_HORIZON_COLOR);

    v_fill_rect(0, PANG_TOP, PANG_LEFT, PANG_FLOOR - PANG_TOP, PANG_FENCE_COLOR);
    v_fill_rect(PANG_RIGHT, PANG_TOP, VIDEO_WIDTH - PANG_RIGHT, PANG_FLOOR - PANG_TOP, PANG_FENCE_COLOR);

    for (y = grass_start + 2; y < PANG_FLOOR; y += 6) {
        for (x = PANG_LEFT + 6; x < PANG_RIGHT - 6; x += 18) {
            v_putpixel(x, y, PANG_GRASS_DOT_COLOR);
        }
    }
}

static void pang_select_params(unsigned char difficulty, PangParams *params)
{
    if (!params) {
        return;
    }

    switch (difficulty) {
    case DIFFICULTY_EASY:
        params->player_speed = 2.6f;
        params->ball_speed_x = 1.0f;
        params->ball_bounce_vy = 4.2f;
        params->gravity = 0.18f;
        params->arrow_speed = 3.0f;
        params->initial_balls = 1;
        break;
    case DIFFICULTY_HARD:
        params->player_speed = 3.4f;
        params->ball_speed_x = 1.8f;
        params->ball_bounce_vy = 5.6f;
        params->gravity = 0.26f;
        params->arrow_speed = 4.0f;
        params->initial_balls = 3;
        break;
    case DIFFICULTY_NORMAL:
    default:
        params->player_speed = 3.0f;
        params->ball_speed_x = 1.4f;
        params->ball_bounce_vy = 5.0f;
        params->gravity = 0.22f;
        params->arrow_speed = 3.5f;
        params->initial_balls = 2;
        break;
    }
}

static void pang_reset_balls(void)
{
    int i;
    int count = g_params.initial_balls;
    int span = PANG_RIGHT - PANG_LEFT - PANG_BALL_XL;

    for (i = 0; i < PANG_MAX_BALLS; ++i) {
        g_balls[i].active = 0;
    }

    if (count < 1) {
        count = 1;
    }
    if (count > PANG_MAX_BALLS) {
        count = PANG_MAX_BALLS;
    }

    for (i = 0; i < count; ++i) {
        float fx = (float)PANG_LEFT + ((float)(i + 1) / (float)(count + 1)) * (float)span;
        float player_center = g_player_x + (float)PANG_PLAYER_W * 0.5f;
        float ball_center = fx + (float)PANG_BALL_XL * 0.5f;
        int dir;

        if (count == 1) {
            fx = (float)PANG_LEFT + 0.75f * (float)span;
            ball_center = fx + (float)PANG_BALL_XL * 0.5f;
        }

        dir = (ball_center < player_center) ? -1 : 1;
        g_balls[i].active = 1;
        g_balls[i].size = PANG_SIZE_XL;
        g_balls[i].x = fx;
        g_balls[i].y = (float)(PANG_TOP + 12);
        g_balls[i].x_prev = g_balls[i].x;
        g_balls[i].y_prev = g_balls[i].y;
        g_balls[i].vx = g_params.ball_speed_x * (float)dir;
        g_balls[i].vy = -pang_ball_bounce_vy(g_balls[i].size);
    }
}

static void pang_spawn_split(const PangBall *source, PangBallSize next_size)
{
    int i;
    int size_old = pang_ball_size_px(source->size);
    int size_new = pang_ball_size_px(next_size);
    float center_x = source->x + (float)size_old * 0.5f;
    float center_y = source->y + (float)size_old * 0.5f;
    float start_x = center_x - (float)size_new * 0.5f;
    float start_y = center_y - (float)size_new * 0.5f;

    for (i = 0; i < PANG_MAX_BALLS; ++i) {
        if (!g_balls[i].active) {
            g_balls[i].active = 1;
            g_balls[i].size = next_size;
            g_balls[i].x = start_x;
            g_balls[i].y = start_y;
            g_balls[i].x_prev = g_balls[i].x;
            g_balls[i].y_prev = g_balls[i].y;
            g_balls[i].vx = g_params.ball_speed_x;
            g_balls[i].vy = -pang_ball_bounce_vy(next_size);
            break;
        }
    }

    for (i = 0; i < PANG_MAX_BALLS; ++i) {
        if (!g_balls[i].active) {
            g_balls[i].active = 1;
            g_balls[i].size = next_size;
            g_balls[i].x = start_x;
            g_balls[i].y = start_y;
            g_balls[i].x_prev = g_balls[i].x;
            g_balls[i].y_prev = g_balls[i].y;
            g_balls[i].vx = -g_params.ball_speed_x;
            g_balls[i].vy = -pang_ball_bounce_vy(next_size);
            break;
        }
    }
}

static int pang_check_player_hit(const PangBall *ball)
{
    int size = pang_ball_size_px(ball->size);
    int margin = pang_ball_margin(ball->size);
    float ball_x = ball->x + (float)margin;
    float ball_y = ball->y + (float)margin;
    float ball_w = (float)(size - margin * 2);
    float ball_h = (float)(size - margin * 2);
    float player_hit_w = 20.0f;
    float player_hit_h = 32.0f;
    float player_hit_x = g_player_x + ((float)PANG_PLAYER_W - player_hit_w) * 0.5f;
    float player_hit_y = g_player_y + ((float)PANG_PLAYER_H - player_hit_h);

    if (ball_x < player_hit_x + player_hit_w &&
        ball_x + ball_w > player_hit_x &&
        ball_y < player_hit_y + player_hit_h &&
        ball_y + ball_h > player_hit_y) {
        return 1;
    }

    return 0;
}

static int pang_check_rope_hit(const PangBall *ball)
{
    int size = pang_ball_size_px(ball->size);
    int margin = pang_ball_margin(ball->size);
    float ball_x = ball->x + (float)margin;
    float ball_y = ball->y + (float)margin;
    float ball_w = (float)(size - margin * 2);
    float ball_h = (float)(size - margin * 2);
    float rope_x = g_arrow_x + (float)PANG_ARROW_W * 0.5f;
    float rope_half = 1.5f;
    float rope_left = rope_x - rope_half;
    float rope_right = rope_x + rope_half;
    float rope_top = g_arrow_y + (float)PANG_ARROW_H;
    float rope_bottom = (float)PANG_FLOOR;

    if (rope_bottom < rope_top) {
        float temp = rope_bottom;
        rope_bottom = rope_top;
        rope_top = temp;
    }

    if (ball_x < rope_right &&
        ball_x + ball_w > rope_left &&
        ball_y < rope_bottom &&
        ball_y + ball_h > rope_top) {
        return 1;
    }

    return 0;
}

static void pang_handle_ball_hit(int index)
{
    PangBallSize next_size = PANG_SIZE_S;
    int spawn_split = 0;

    if (!g_balls[index].active) {
        return;
    }

    if (g_balls[index].size == PANG_SIZE_XL) {
        next_size = PANG_SIZE_M;
        spawn_split = 1;
    } else if (g_balls[index].size == PANG_SIZE_M) {
        next_size = PANG_SIZE_S;
        spawn_split = 1;
    }

    g_score += (uint64_t)pang_score_for_hit(g_balls[index].size);
    g_balls[index].active = 0;

    if (g_sound_enabled) {
        sound_play_tone(740, 30);
    }

    if (spawn_split) {
        pang_spawn_split(&g_balls[index], next_size);
        if (g_sound_enabled) {
            sound_play_tone(620, 40);
        }
    }
}

static void pang_fire_arrow(void)
{
    g_arrow_active = 1;
    g_arrow_x = g_player_x + ((float)PANG_PLAYER_W - (float)PANG_ARROW_W) * 0.5f;
    g_arrow_y = g_player_y - (float)PANG_ARROW_H;
    g_arrow_x_prev = g_arrow_x;
    g_arrow_y_prev = g_arrow_y;

    if (g_sound_enabled) {
        sound_play_tone(520, 25);
    }
}

void Pang_Init(const GameSettings *settings)
{
    if (settings) {
        g_settings = *settings;
    } else {
        memset(&g_settings, 0, sizeof(g_settings));
    }

    pang_select_params(g_settings.difficulty, &g_params);
    g_params.player_speed *= g_settings.speed_multiplier;
    g_params.ball_speed_x *= g_settings.speed_multiplier;
    g_params.ball_bounce_vy *= g_settings.speed_multiplier;
    g_params.gravity *= g_settings.speed_multiplier;
    g_params.arrow_speed *= g_settings.speed_multiplier;

        // OJO: ralentización global fija (0.25 = muy lento, 0.50 = normal)
    {
        const float slow = 0.25f;

        g_params.player_speed *= slow;
        g_params.ball_speed_x *= slow;
        g_params.ball_bounce_vy *= slow;
        g_params.arrow_speed *= slow;

        // Mantiene rebotes altos: gravedad al cuadrado
        g_params.gravity *= (slow * slow);
    }

    pang_load_sprites();

    g_finished = 0;
    g_did_win = 0;
    g_sound_enabled = g_settings.sound_enabled ? 1 : 0;
    g_use_keyboard = 1;
    if (g_settings.input_mode == INPUT_JOYSTICK) {
        if (in_joystick_available()) {
            g_use_keyboard = 0;
        }
    }

    g_player_x = (float)(PANG_LEFT + (PANG_RIGHT - PANG_LEFT - PANG_PLAYER_W) / 2);
    g_player_x_prev = g_player_x;
    g_player_y = (float)(PANG_FLOOR - PANG_PLAYER_H);
    g_player_dir = 1;
    g_player_move_dir = 0;
    g_walk_ticks = 0;
    g_walk_frame = 0;

    g_arrow_active = 0;
    g_fire_held = 0;

    g_score = 0;
    g_elapsed_ticks = 0;

    pang_reset_balls();

    g_end_detail[0] = '\0';

    Pang_StorePreviousState();

    while (in_keyhit()) {
        in_poll();
    }
}

void Pang_StorePreviousState(void)
{
    int i;

    g_player_x_prev = g_player_x;
    g_arrow_x_prev = g_arrow_x;
    g_arrow_y_prev = g_arrow_y;

    for (i = 0; i < PANG_MAX_BALLS; ++i) {
        if (g_balls[i].active) {
            g_balls[i].x_prev = g_balls[i].x;
            g_balls[i].y_prev = g_balls[i].y;
        }
    }
}

void Pang_Update(void)
{
    int move_dir = 0;
    int fire_down = 0;
    int fire_pressed = 0;
    int i;
    int any_balls = 0;

    if (g_finished) {
        return;
    }

    g_elapsed_ticks++;

    {
        uint32_t limit = pang_time_limit_seconds(g_settings.difficulty);
        uint32_t limit_ticks = limit * PANG_TICKS_PER_SECOND;
        if (limit > 0 && g_elapsed_ticks >= limit_ticks) {
            g_finished = 1;
            g_did_win = 0;
            snprintf(g_end_detail, sizeof(g_end_detail), "TIEMPO AGOTADO");
            if (g_sound_enabled) {
                sound_play_tone(220, 160);
            }
            return;
        }
    }

    sound_update();

    if (g_use_keyboard) {
        if (kb_down(SC_LEFT) || kb_down(SC_A)) {
            move_dir -= 1;
        }
        if (kb_down(SC_RIGHT) || kb_down(SC_D)) {
            move_dir += 1;
        }
        if (kb_down(SC_SPACE) || kb_down(SC_LCTRL)) {
            fire_down = 1;
        }
    } else {
        int dx = 0;
        int dy = 0;
        unsigned char buttons = 0;

        if (in_joystick_direction(&dx, &dy, &buttons)) {
            move_dir = dx;
            if (buttons & 1) {
                fire_down = 1;
            }
        }
    }

    fire_pressed = fire_down && !g_fire_held;
    g_fire_held = fire_down;

    if (move_dir != 0) {
        g_player_dir = (move_dir < 0) ? -1 : 1;
        g_player_move_dir = move_dir;
        g_player_x += (float)move_dir * g_params.player_speed;
        g_player_x = clampf(g_player_x, (float)PANG_LEFT, (float)(PANG_RIGHT - PANG_PLAYER_W));
        g_walk_ticks++;
        if (g_walk_ticks >= PANG_WALK_TICKS) {
            g_walk_ticks = 0;
            g_walk_frame ^= 1;
        }
    } else {
        g_player_move_dir = 0;
        g_walk_ticks = 0;
        g_walk_frame = 0;
    }

    if (fire_pressed && !g_arrow_active) {
        pang_fire_arrow();
    }

    if (g_arrow_active) {
        g_arrow_y -= g_params.arrow_speed;
        if (g_arrow_y <= (float)PANG_TOP) {
            g_arrow_active = 0;
        }
    }

    for (i = 0; i < PANG_MAX_BALLS; ++i) {
        PangBall *ball = &g_balls[i];
        int size;
        float next_x;
        float next_y;

        if (!ball->active) {
            continue;
        }

        any_balls = 1;

        ball->vy += g_params.gravity;
        next_x = ball->x + ball->vx;
        next_y = ball->y + ball->vy;
        size = pang_ball_size_px(ball->size);

        if (next_x <= (float)PANG_LEFT) {
            next_x = (float)PANG_LEFT;
            ball->vx = (ball->vx < 0.0f) ? -ball->vx : ball->vx;
        } else if (next_x + (float)size >= (float)PANG_RIGHT) {
            next_x = (float)(PANG_RIGHT - size);
            ball->vx = (ball->vx > 0.0f) ? -ball->vx : ball->vx;
        }

        if (next_y <= (float)PANG_TOP) {
            next_y = (float)PANG_TOP;
            ball->vy = (ball->vy < 0.0f) ? -ball->vy : ball->vy;
        } else if (next_y + (float)size >= (float)PANG_FLOOR) {
            next_y = (float)(PANG_FLOOR - size);
            ball->vy = -pang_ball_bounce_vy(ball->size);
        }

        ball->x = next_x;
        ball->y = next_y;

        if (pang_check_player_hit(ball)) {
            g_finished = 1;
            g_did_win = 0;
            if (g_sound_enabled) {
                sound_play_tone(220, 160);
            }
            return;
        }
    }

    if (g_arrow_active) {
        for (i = 0; i < PANG_MAX_BALLS; ++i) {
            if (!g_balls[i].active) {
                continue;
            }
            if (pang_check_rope_hit(&g_balls[i])) {
                g_arrow_active = 0;
                pang_handle_ball_hit(i);
                break;
            }
        }
    }

    if (!any_balls) {
        uint32_t secs = g_elapsed_ticks / PANG_TICKS_PER_SECOND;
        uint32_t target = pang_time_target_seconds(g_settings.difficulty);

        if (secs < target) {
            uint32_t bonus = (target - secs) * PANG_TIME_BONUS_PER_SEC;
            g_score += (uint64_t)bonus;
        }

        g_finished = 1;
        g_did_win = 1;
        if (g_sound_enabled) {
            sound_play_tone(880, 120);
        }
    }
}

static const PangSprite *pang_ball_sprite(PangBallSize size)
{
    switch (size) {
    case PANG_SIZE_XL:
        return &g_ball_xl;
    case PANG_SIZE_M:
        return &g_ball_m;
    case PANG_SIZE_S:
    default:
        return &g_ball_s;
    }
}

void Pang_DrawInterpolated(float alpha)
{
    char hud[64];
    char score_text[24];
    char timer_text[24];
    uint32_t limit_seconds = pang_time_limit_seconds(g_settings.difficulty);
    uint32_t elapsed_seconds = g_elapsed_ticks / PANG_TICKS_PER_SECOND;
    uint32_t remaining_seconds = 0;
    int i;
    float player_x = lerpf(g_player_x_prev, g_player_x, alpha);
    float arrow_x = lerpf(g_arrow_x_prev, g_arrow_x, alpha);
    float arrow_y = lerpf(g_arrow_y_prev, g_arrow_y, alpha);
    int player_x_i = (int)(player_x + 0.5f);
    int player_y_i = (int)(g_player_y + 0.5f);
    const PangSprite *player_sprite = &g_player1;

    pang_draw_background();
    v_fill_rect(0, 0, VIDEO_WIDTH, 16, PANG_HUD_BG_COLOR);
    v_fill_rect(0, 184, VIDEO_WIDTH, 16, PANG_HUD_BG_COLOR);

    if (g_arrow_active) {
        int rope_x = (int)(arrow_x + (float)PANG_ARROW_W * 0.5f);
        int rope_top = (int)(arrow_y + (float)PANG_ARROW_H);
        int rope_bottom = (int)PANG_FLOOR;
        int y;

        if (rope_bottom < rope_top) {
            int tmp = rope_bottom;
            rope_bottom = rope_top;
            rope_top = tmp;
        }

        for (y = rope_top; y < rope_bottom; ++y) {
            v_putpixel(rope_x, y, PANG_LINE_COLOR);
        }
    }

    for (i = 0; i < PANG_MAX_BALLS; ++i) {
        const PangBall *ball = &g_balls[i];
        const PangSprite *sprite;
        float x;
        float y;

        if (!ball->active) {
            continue;
        }

        sprite = pang_ball_sprite(ball->size);
        x = lerpf(ball->x_prev, ball->x, alpha);
        y = lerpf(ball->y_prev, ball->y, alpha);
        pang_blit_sprite((int)(x + 0.5f), (int)(y + 0.5f), sprite);
    }

    if (g_player_move_dir != 0) {
        player_sprite = (g_walk_frame == 0) ? &g_player2 : &g_player3;
    }

    if (g_player_dir < 0) {
        pang_blit_sprite_flipped(player_x_i, player_y_i, player_sprite, 1);
    } else {
        pang_blit_sprite(player_x_i, player_y_i, player_sprite);
    }

    if (g_arrow_active) {
        pang_blit_sprite((int)(arrow_x + 0.5f), (int)(arrow_y + 0.5f), &g_arrow);
    }

    if (elapsed_seconds < limit_seconds) {
        remaining_seconds = limit_seconds - elapsed_seconds;
    }

    snprintf(hud, sizeof(hud), "D:%s x%.2f", difficulty_short(g_settings.difficulty), g_settings.speed_multiplier);
    v_puts(0, 0, "1989", 7);
    v_puts(VIDEO_WIDTH - (text_len(hud) * 8), 0, hud, 7);

    snprintf(timer_text, sizeof(timer_text), "TIEMPO %u", (unsigned)remaining_seconds);
    v_puts(8, 188, timer_text, 15);

    high_scores_format_score(score_text, sizeof(score_text), g_score);
    v_puts(VIDEO_WIDTH - (text_len(score_text) * 8) - 8, 188, score_text, 15);

    v_present();
}

void Pang_End(void)
{
    char score_text[16];

    high_scores_format_score(score_text, sizeof(score_text), g_score);
    snprintf(g_end_detail, sizeof(g_end_detail), "PUNTOS %s", score_text);
    pang_free_sprites();
}

int Pang_IsFinished(void)
{
    return g_finished;
}

int Pang_DidWin(void)
{
    return g_did_win;
}

const char *Pang_GetEndDetail(void)
{
    return g_end_detail;
}

uint64_t Pang_GetScore(void)
{
    return g_score;
}
