#include "flappy.h"

#include "../../CORE/input.h"
#include "../../CORE/options.h"
#include "../../CORE/sound.h"
#include "../../CORE/video.h"
#include "../../CORE/sprite_dat.h"
#include "../../CORE/keyboard.h"
#include "../../CORE/high_scores.h"

#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLAPPY_GAME_W 320
#define FLAPPY_GAME_H 152
#define FLAPPY_AD_Y 152
#define FLAPPY_AD_H 48

#define FLAPPY_PLAYER_W 24
#define FLAPPY_PLAYER_H 16
#define FLAPPY_PLAYER_X 70

#define FLAPPY_PIPE_W 32
#define FLAPPY_MAX_PIPES 6
#define FLAPPY_MIN_GAP_Y 18
#define FLAPPY_GAP_MARGIN 18

#define FLAPPY_SKY_COLOR 148
#define FLAPPY_GROUND_COLOR 3
#define FLAPPY_PIPE_COLOR 34
#define FLAPPY_PIPE_DARK 2
#define FLAPPY_PIPE_LIGHT 37
#define FLAPPY_HUD_COLOR 15

#define FLAPPY_GLOBAL_SPEED_SCALE 0.4f

// Ajuste global, no depende de dificultad
#define FLAPPY_GRAVITY_TUNE   0.55f   // < 1.0 = cae más lento
#define FLAPPY_JUMP_TUNE      1.0f   // < 1.0 = salto menos bestia
#define FLAPPY_TERMINAL_VY    3.2f    // Velocidad máxima de caída en px/frame

static float g_terminal_vy = 0.0f;

#define FLAPPY_PLAYER_MAX_PIXELS 4096UL
static const char *g_ad_texts[] = {
    "BUY MORE ENERGY! 0.99!",
    "SPECIAL OFFER! VPN 1 YEAR, ONLY 49.99!",
    "NEW PHONE. SAME CHARGER? NO.",
    "UNLIMITED SCROLL. ZERO JOY.",
    "MEGA CAMERA. TINY BATTERY.",
    "TIRED OF THIS? BUY THE NO-ADS."
};

static const unsigned char g_ad_text_colors[] = {
    10,
    11,
    14,
    12,
    13
};

typedef enum {
    FLAPPY_STATE_READY = 0,
    FLAPPY_STATE_PLAYING,
    FLAPPY_STATE_DEAD
} FlappyState;

typedef struct {
    float pipe_speed;
    int gap_height;
    int spacing;
    float gravity;
    float jump_vy;
    int min_pipes;
} FlappyParams;

typedef struct {
    unsigned short w;
    unsigned short h;
    unsigned long max_pixels;
    unsigned char far *pixels;
} FlappySprite;

typedef struct {
    float x;
    float prev_x;
    int gap_y;
    int scored;
} FlappyPipe;

static GameSettings g_settings;
static FlappyParams g_params;

static FlappySprite g_player_sprite;
static FlappyPipe g_pipes[FLAPPY_MAX_PIPES];
static int g_pipe_count = 0;
static float g_rightmost_pipe_x = 0.0f;

static FlappyState g_state = FLAPPY_STATE_READY;
static int g_finished = 0;
static int g_did_win = 0;
static int g_use_keyboard = 1;
static int g_sound_enabled = 0;
static int g_jump_held = 0;
static int g_ad_text_index = 0;

static float g_player_x = 0.0f;
static float g_player_y = 0.0f;
static float g_player_y_prev = 0.0f;
static float g_player_vy = 0.0f;

static uint64_t g_score = 0;
static uint64_t g_final_score = 0;
static char g_end_detail[32] = "";

// Caché de texto para rendimiento en DOS
static char g_hud_cached[64];
static int g_hud_dirty = 1;

static char g_score_cached[7] = "000000";
static unsigned long g_score_last = 0xFFFFFFFFUL;

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

static int rand_range(int min_value, int max_value)
{
    if (max_value <= min_value) {
        return min_value;
    }
    return min_value + (rand() % (max_value - min_value + 1));
}

static void flappy_select_params(unsigned char difficulty, FlappyParams *params)
{
    if (!params) {
        return;
    }

    switch (difficulty) {
    case DIFFICULTY_EASY:
        params->pipe_speed = 1.5f;
        params->gap_height = 58;
        params->spacing = 140;
        params->gravity = 0.25f;
        params->jump_vy = -3.5f;
        params->min_pipes = 3;
        break;
    case DIFFICULTY_HARD:
        params->pipe_speed = 3.5f;
        params->gap_height = 40;
        params->spacing = 110;
        params->gravity = 0.33f;
        params->jump_vy = -3.8f;
        params->min_pipes = 10;
        break;
    case DIFFICULTY_NORMAL:
    default:
        params->pipe_speed = 2.5f;
        params->gap_height = 48;
        params->spacing = 120;
        params->gravity = 0.29f;
        params->jump_vy = -3.6f;
        params->min_pipes = 5;
        break;
    }
}

static int flappy_alloc_sprite_pixels(FlappySprite *sprite)
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

static int flappy_load_sprite(const char *path, FlappySprite *sprite)
{
    if (!sprite || !path) {
        return 0;
    }

    sprite->w = 0;
    sprite->h = 0;

    if (!flappy_alloc_sprite_pixels(sprite)) {
        return 0;
    }

    return sprite_dat_load_auto(path, &sprite->w, &sprite->h, sprite->pixels, sprite->max_pixels);
}

static void flappy_free_sprite(FlappySprite *sprite)
{
    if (!sprite || !sprite->pixels) {
        return;
    }

    _ffree(sprite->pixels);
    sprite->pixels = NULL;
    sprite->w = 0;
    sprite->h = 0;
}

static void flappy_pick_ad_text(void)
{
    int count = (int)(sizeof(g_ad_texts) / sizeof(g_ad_texts[0]));
    if (count <= 0) {
        g_ad_text_index = 0;
        return;
    }
    g_ad_text_index = rand() % count;
}

static int flappy_jump_pressed(void)
{
    int pressed = 0;

    if (g_use_keyboard) {
        if (Input_Pressed(SC_SPACE) || Input_Pressed(SC_LCTRL)) {
            pressed = 1;
        }
    } else {
        unsigned char buttons = 0;
        int dx = 0;
        int dy = 0;

        if (in_joystick_direction(&dx, &dy, &buttons)) {
            int down = (buttons & 1) ? 1 : 0;
            pressed = down && !g_jump_held;
            g_jump_held = down;
        }
    }

    return pressed;
}

static int flappy_rects_intersect(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    return (ax < (bx + bw)) && ((ax + aw) > bx) && (ay < (by + bh)) && ((ay + ah) > by);
}

static int flappy_random_gap_y(void)
{
    int max_gap_y = FLAPPY_GAME_H - g_params.gap_height - FLAPPY_GAP_MARGIN;
    return rand_range(FLAPPY_MIN_GAP_Y, max_gap_y);
}

static void flappy_update_score_cached(void)
{
    unsigned long s = (unsigned long)g_score;
    if (s > 999999UL) {
        s %= 1000000UL;
    }

    g_score_cached[5] = (char)('0' + (s % 10));
    s /= 10;
    g_score_cached[4] = (char)('0' + (s % 10));
    s /= 10;
    g_score_cached[3] = (char)('0' + (s % 10));
    s /= 10;
    g_score_cached[2] = (char)('0' + (s % 10));
    s /= 10;
    g_score_cached[1] = (char)('0' + (s % 10));
    s /= 10;
    g_score_cached[0] = (char)('0' + (s % 10));
    g_score_cached[6] = 0;
}

// Construye "D:XX xN.NN" sin printf
static void flappy_rebuild_hud(void)
{
    int mult100 = (int)(g_settings.speed_multiplier * 100.0f + 0.5f);

    // OJO: si mult100 es 0, fuerza 1.00
    if (mult100 <= 0) {
        mult100 = 100;
    }

    {
        const char *d = difficulty_short(g_settings.difficulty);
        char *p = g_hud_cached;

        *p++ = 'D';
        *p++ = ':';
        while (*d) {
            *p++ = *d++;
        }

        *p++ = ' ';
        *p++ = 'x';

        // Formato N.NN
        *p++ = (char)('0' + (mult100 / 100));
        *p++ = '.';
        *p++ = (char)('0' + ((mult100 / 10) % 10));
        *p++ = (char)('0' + (mult100 % 10));

        *p = 0;
    }

    g_hud_dirty = 0;
}

static void flappy_init_pipes(void)
{
    int i;
    int start_x = FLAPPY_GAME_W + 40;

    g_pipe_count = FLAPPY_MAX_PIPES;
    for (i = 0; i < g_pipe_count; ++i) {
        g_pipes[i].x = (float)(start_x + (i * g_params.spacing));
        g_pipes[i].prev_x = g_pipes[i].x;
        g_pipes[i].gap_y = flappy_random_gap_y();
        g_pipes[i].scored = 0;
    }
    g_rightmost_pipe_x = g_pipes[g_pipe_count - 1].x;
}

static void flappy_update_pipes(void)
{
    int i;
    float step = g_params.pipe_speed;
    g_rightmost_pipe_x -= step;

    for (i = 0; i < g_pipe_count; ++i) {
        FlappyPipe *pipe = &g_pipes[i];

        pipe->x -= step;

        if ((pipe->x + FLAPPY_PIPE_W) < 0.0f) {
            pipe->x = g_rightmost_pipe_x + g_params.spacing;
            g_rightmost_pipe_x = pipe->x;
            pipe->gap_y = flappy_random_gap_y();
            pipe->scored = 0;
        }

        if (!pipe->scored && g_player_x > (pipe->x + FLAPPY_PIPE_W)) {
            pipe->scored = 1;
            g_score++;
            if (g_sound_enabled) {
                sound_play_tone(700, 30);
            }

        }
    }
}

static void flappy_quick_restart(void)
{
    g_state = FLAPPY_STATE_READY;
    g_finished = 0;
    g_did_win = 0;

    g_jump_held = 0;

    g_player_x = (float)FLAPPY_PLAYER_X;
    g_player_y = (float)((FLAPPY_GAME_H - FLAPPY_PLAYER_H) / 2);
    g_player_y_prev = g_player_y;
    g_player_vy = 0.0f;

    g_score = 0;
    g_final_score = 0;
    g_end_detail[0] = '\0';
    g_hud_dirty = 1;
    g_score_last = 0xFFFFFFFFUL;

    flappy_init_pipes();

    // Recarga anuncio también en reinicio rápido
    flappy_pick_ad_text();
}

static void flappy_die(void)
{
    if (g_state == FLAPPY_STATE_DEAD) {
        return;
    }

    // OJO: si no hay puntos, cuenta como derrota y reinicia
    if (g_score < g_params.min_pipes) {
        if (g_sound_enabled) {
            sound_play_tone(180, 120);
        }
        flappy_quick_restart();
        return;
    }

    g_state = FLAPPY_STATE_DEAD;
    g_finished = 1;
    g_did_win = 1;
    g_final_score = g_score;

    if (g_sound_enabled) {
        sound_play_tone(180, 120);
    }
}

static void flappy_check_collisions(void)
{
    int i;
    int player_x = (int)g_player_x + 4;
    int player_y = (int)g_player_y + 3;
    int player_w = 16;
    int player_h = 10;

    for (i = 0; i < g_pipe_count; ++i) {
        FlappyPipe *pipe = &g_pipes[i];
        int pipe_x = (int)pipe->x;
        int gap_y = pipe->gap_y;
        int top_h = gap_y;
        int bottom_y = gap_y + g_params.gap_height;
        int bottom_h = FLAPPY_GAME_H - bottom_y;

        if ((pipe_x + FLAPPY_PIPE_W) < 0 || pipe_x >= FLAPPY_GAME_W) {
            continue;
        }

        if (top_h > 0) {
            if (flappy_rects_intersect(player_x, player_y, player_w, player_h, pipe_x, 0,
                                       FLAPPY_PIPE_W, top_h)) {
                flappy_die();
                return;
            }
        }

        if (bottom_h > 0) {
            if (flappy_rects_intersect(player_x, player_y, player_w, player_h, pipe_x, bottom_y,
                                       FLAPPY_PIPE_W, bottom_h)) {
                flappy_die();
                return;
            }
        }
    }
}

static void flappy_draw_pipe_segment(int x, int y, int h)
{
    if (h <= 0) {
        return;
    }

    v_fill_rect(x, y, FLAPPY_PIPE_W, h, FLAPPY_PIPE_COLOR);
    v_fill_rect(x, y, 2, h, FLAPPY_PIPE_DARK);
    v_fill_rect(x + FLAPPY_PIPE_W - 2, y, 2, h, FLAPPY_PIPE_DARK);
    if (h > 2) {
        v_fill_rect(x + 2, y + 1, 1, h - 2, FLAPPY_PIPE_LIGHT);
    }
}

static void flappy_draw_pipe(float x, int gap_y)
{
    int pipe_x = (int)x;
    int top_h = gap_y;
    int bottom_y = gap_y + g_params.gap_height;
    int bottom_h = FLAPPY_GAME_H - bottom_y;

    flappy_draw_pipe_segment(pipe_x, 0, top_h);
    flappy_draw_pipe_segment(pipe_x, bottom_y, bottom_h);
}

static void flappy_draw_center_text(const char *text, int y, unsigned char color)
{
    int len = 0;
    int x;

    while (text[len] != '\0') {
        len++;
    }

    x = (VIDEO_WIDTH - (len * 8)) / 2;
    v_puts(x, y, text, color);
}

static void flappy_draw_ad_text(void)
{
    int text_y = FLAPPY_AD_Y + ((FLAPPY_AD_H - 8) / 2);
    int count = (int)(sizeof(g_ad_texts) / sizeof(g_ad_texts[0]));
    unsigned char color = g_ad_text_colors[g_ad_text_index % (int)(sizeof(g_ad_text_colors) /
        sizeof(g_ad_text_colors[0]))];

    if (count <= 0) {
        return;
    }

    flappy_draw_center_text(g_ad_texts[g_ad_text_index % count], text_y, color);
}

void Flappy_Init(const GameSettings *settings)
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

    g_sound_enabled = g_settings.sound_enabled ? 1 : 0;

    flappy_select_params(g_settings.difficulty, &g_params);

    {
        float mul = g_settings.speed_multiplier * FLAPPY_GLOBAL_SPEED_SCALE;

        g_params.pipe_speed *= mul;

        // Física base escalada
        g_params.gravity *= mul;
        g_params.jump_vy *= mul;

        // Ajuste global aplicado tras el escalado
        g_params.gravity *= FLAPPY_GRAVITY_TUNE;
        g_params.jump_vy *= FLAPPY_JUMP_TUNE;

        // Velocidad terminal escalada con el mul
        g_terminal_vy = FLAPPY_TERMINAL_VY * mul;
    }


    g_state = FLAPPY_STATE_READY;
    g_finished = 0;
    g_did_win = 0;
    g_jump_held = 0;

    g_player_x = (float)FLAPPY_PLAYER_X;
    g_player_y = (float)((FLAPPY_GAME_H - FLAPPY_PLAYER_H) / 2);
    g_player_y_prev = g_player_y;
    g_player_vy = 0.0f;

    g_score = 0;
    g_final_score = 0;
    g_end_detail[0] = '\0';
    g_hud_dirty = 1;
    g_score_last = 0xFFFFFFFFUL;

    g_player_sprite.max_pixels = FLAPPY_PLAYER_MAX_PIXELS;
    flappy_load_sprite("SPRITES\\flappy.dat", &g_player_sprite);
    flappy_pick_ad_text();

    flappy_init_pipes();

    Flappy_StorePreviousState();

    while (in_keyhit()) {
        in_poll();
    }
}

void Flappy_StorePreviousState(void)
{
    int i;

    g_player_y_prev = g_player_y;

    for (i = 0; i < g_pipe_count; ++i) {
        g_pipes[i].prev_x = g_pipes[i].x;
    }
}

void Flappy_Update(void)
{
    int jump_pressed;

    if (g_finished) {
        return;
    }

    sound_update();

    jump_pressed = flappy_jump_pressed();

    if (g_state == FLAPPY_STATE_READY) {
        if (jump_pressed) {
            g_state = FLAPPY_STATE_PLAYING;
            g_player_vy = g_params.jump_vy;
            if (g_sound_enabled) {
                sound_play_tone(520, 35);
            }
        }
        return;
    }

    if (g_state != FLAPPY_STATE_PLAYING) {
        return;
    }

    if (jump_pressed) {
        g_player_vy = g_params.jump_vy;
        if (g_sound_enabled) {
            sound_play_tone(520, 35);
        }
    }

    g_player_vy += g_params.gravity;

    // OJO: clamp de caída para evitar caída infinita
    if (g_player_vy > g_terminal_vy) {
        g_player_vy = g_terminal_vy;
    }

    g_player_y += g_player_vy;


    if (g_player_y < 0.0f) {
        g_player_y = 0.0f;
        flappy_die();
        return;
    }

    if (g_player_y > (float)(FLAPPY_GAME_H - FLAPPY_PLAYER_H)) {
        g_player_y = (float)(FLAPPY_GAME_H - FLAPPY_PLAYER_H);
        flappy_die();
        return;
    }

    flappy_update_pipes();
    flappy_check_collisions();
}

void Flappy_DrawInterpolated(float alpha)
{
    int i;
    int player_y;

    // OJO: en 486 la interpolación con float mata, se ignora
    (void)alpha;

    player_y = (int)g_player_y;

    // Área de juego sin pisar la barra superior
    v_fill_rect(0, 8, FLAPPY_GAME_W, FLAPPY_GAME_H - 8, FLAPPY_SKY_COLOR);
    v_fill_rect(0, FLAPPY_GAME_H - 1, FLAPPY_GAME_W, 1, FLAPPY_GROUND_COLOR);
    v_fill_rect(0, 0, VIDEO_WIDTH, 8, 0);
    v_fill_rect(0, FLAPPY_AD_Y, FLAPPY_GAME_W, FLAPPY_AD_H, 0);

    for (i = 0; i < g_pipe_count; ++i) {
        int pipe_x = (int)g_pipes[i].x;

        if ((pipe_x + FLAPPY_PIPE_W) < 0 || pipe_x >= FLAPPY_GAME_W) {
            continue;
        }
        flappy_draw_pipe((float)pipe_x, g_pipes[i].gap_y);
        // Convierte a float una sola vez para el blit
    }

    if (g_player_sprite.pixels) {
        v_blit_sprite((int)g_player_x, player_y, g_player_sprite.w, g_player_sprite.h,
                      g_player_sprite.pixels, 0);
    } else {
        v_fill_rect((int)g_player_x, player_y, FLAPPY_PLAYER_W, FLAPPY_PLAYER_H, 15);
    }

    // HUD
    v_puts(0, 0, "2013", 7);

    if (g_hud_dirty) flappy_rebuild_hud();
    v_puts(VIDEO_WIDTH - (text_len(g_hud_cached) * 8), 0, g_hud_cached, 7);

    // Puntuación en caché
    if ((unsigned long)g_score != g_score_last) {
        g_score_last = (unsigned long)g_score;
        flappy_update_score_cached();
    }
    flappy_draw_center_text(g_score_cached, 0, FLAPPY_HUD_COLOR);

    flappy_draw_ad_text();

    v_present();
}

void Flappy_End(void)
{
    char score_text[16];

    g_final_score = g_score;
    high_scores_format_score(score_text, sizeof(score_text), g_score);
    snprintf(g_end_detail, sizeof(g_end_detail), "PUNTOS %s", score_text);

    flappy_free_sprite(&g_player_sprite);
}

int Flappy_IsFinished(void)
{
    return g_finished;
}

int Flappy_DidWin(void)
{
    return g_did_win;
}

const char *Flappy_GetEndDetail(void)
{
    return g_end_detail;
}

uint64_t Flappy_GetScore(void)
{
    return g_final_score;
}
