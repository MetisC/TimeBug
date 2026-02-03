#include "invaders.h"

#include "../../CORE/input.h"
#include "../../CORE/options.h"
#include "../../CORE/sound.h"
#include "../../CORE/video.h"
#include "../../CORE/keyboard.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INVADER_COLS 8
#define INVADER_W 8
#define INVADER_H 6
#define INVADER_SPACING_X 6
#define INVADER_SPACING_Y 6
#define INVADER_START_Y 32
#define PLAYER_Y (VIDEO_HEIGHT - 18)
#define PLAYER_SHOT_W 2
#define PLAYER_SHOT_H 4
#define ENEMY_SHOT_W 2
#define ENEMY_SHOT_H 4
#define INVADER_TICKS_PER_SECOND 60
#define INVADER_ANIM_INTERVAL_BASE_SECONDS 0.50f
#define INVADER_ANIM_INTERVAL_BASE_TICKS \
    ((int)(INVADER_ANIM_INTERVAL_BASE_SECONDS * INVADER_TICKS_PER_SECOND))
#define EXPLOSION_PARTICLE_MAX 32
#define EXPLOSION_PARTICLE_LIFE 10

typedef struct {
    float player_speed;
    float player_shot_speed;
    float enemy_speed;
    float enemy_speed_max;
    float enemy_accel;
    float enemy_shot_speed;
    int step_down;
    int target_seconds;
    int enemy_fire_interval;
    int formation_margin_x;
    int player_shot_cooldown_ticks;
    int player_w;
    int player_h;
} InvaderParams;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    int life;
    int active;
} ExplosionParticle;

static GameSettings g_settings;
static InvaderParams g_params;
static int g_finished = 0;
static int g_did_win = 0;
static int g_sound_enabled = 0;
static int g_use_keyboard = 1;

static int g_enemies[INVADER_ROWS][INVADER_COLS];
static float g_form_x = 0.0f;
static float g_form_y = 0.0f;
static float g_form_x_prev = 0.0f;
static float g_form_y_prev = 0.0f;
static int g_direction = 1;
static float g_enemy_speed = 0.0f;

static float g_player_x = 0.0f;
static float g_player_x_prev = 0.0f;

static int g_player_shot_active = 0;
static float g_player_shot_x = 0.0f;
static float g_player_shot_y = 0.0f;
static float g_player_shot_x_prev = 0.0f;
static float g_player_shot_y_prev = 0.0f;
static int g_player_shot_cooldown = 0;
static int g_fire_held = 0;

static int g_enemy_shot_active = 0;
static float g_enemy_shot_x = 0.0f;
static float g_enemy_shot_y = 0.0f;
static float g_enemy_shot_x_prev = 0.0f;
static float g_enemy_shot_y_prev = 0.0f;
static int g_invaders_alive = 0;
static int g_inv_anim_ticks = 0;
static int g_inv_anim_frame = 0; // Índice: 0 = INV_SPR_CRAB_A, 1 = INV_SPR_CRAB_B
static unsigned long g_score = 0;
static char g_end_detail[32] = "";

static int g_elapsed_ticks = 0;
static int g_target_ticks = 0;
static ExplosionParticle g_explosion_particles[EXPLOSION_PARTICLE_MAX];

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

static void draw_rect(int x, int y, int w, int h, unsigned char color)
{
    int ix;
    int iy;

    for (iy = 0; iy < h; ++iy) {
        for (ix = 0; ix < w; ++ix) {
            v_putpixel(x + ix, y + iy, color);
        }
    }
}

// Sprite 8x6 A
static const unsigned char INV_SPR_CRAB_A[6] = {
    0x24, // ..#..#..
    0x5A, // .#.##.#.
    0xDB, // ##.##.##
    0xFF, // ########
    0x5A, // .#.##.#.
    0x24  // ..#..#..
};

// Sprite 8x6 B
static const unsigned char INV_SPR_CRAB_B[6] = {
    0x24, // ..#..#..
    0x5A, // .#.##.#.
    0xDB, // ##.##.##
    0xFF, // ########
    0x5A, // .#.##.#.
    0x81  // #......#
};

static void draw_mask_8x6(int x, int y, const unsigned char *rows, unsigned char color)
{
    int iy, ix;

    for (iy = 0; iy < 6; ++iy) {
        unsigned char m = rows[iy];
        for (ix = 0; ix < 8; ++ix) {
            if (m & (1u << (7 - ix))) {
                v_putpixel(x + ix, y + iy, color);
            }
        }
    }
}

static int invaders_player_y(void)
{
    return PLAYER_Y;
}

static void draw_center_text(const char *text, int y, unsigned char color)
{
    int len = 0;
    int x;

    while (text && text[len] != '\0') {
        len++;
    }

    x = (VIDEO_WIDTH - (len * 8)) / 2;
    v_puts(x, y, text, color);
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

static void invaders_format_score(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return;
    }
    snprintf(buffer, buffer_size, "PUNTOS %07lu", g_score);
}

static void invaders_select_params(unsigned char difficulty, InvaderParams *params)
{
    if (!params) {
        return;
    }

    switch (difficulty) {
    case DIFFICULTY_EASY:
        params->player_speed = 2.2f;
        params->player_shot_speed = 3.2f;
        params->enemy_speed = 0.6f;          // Ajuste de dificultad
        params->enemy_speed_max = 2.2f;      // Límite máximo suavizado
        params->enemy_accel = 0.0007f;
        params->enemy_shot_speed = 1.4f;
        params->step_down = 6;               // OJO: entero
        params->target_seconds = 34;
        params->enemy_fire_interval = 120;
        params->formation_margin_x = 6;
        params->player_shot_cooldown_ticks = 12;
        params->player_w = 24;
        params->player_h = 6;
        break;

    case DIFFICULTY_HARD:
        params->player_speed = 2.9f;
        params->player_shot_speed = 3.6f;
        params->enemy_speed = 0.6f;
        params->enemy_speed_max = 2.0f;
        params->enemy_accel = 0.0007f;
        params->enemy_shot_speed = 2.0f;
        params->step_down = 8;               // OJO: entero
        params->target_seconds = 46;
        params->enemy_fire_interval = 45;
        params->formation_margin_x = 6;
        params->player_shot_cooldown_ticks = 12;
        params->player_w = 20;
        params->player_h = 6;
        break;

    case DIFFICULTY_NORMAL:
    default:
        params->player_speed = 2.5f;
        params->player_shot_speed = 3.4f;
        params->enemy_speed = 0.6f;          // Base común
        params->enemy_speed_max = 2.0f;
        params->enemy_accel = 0.0007f;
        params->enemy_shot_speed = 1.5f;
        params->step_down = 6;               // OJO: entero
        params->target_seconds = 40;
        params->enemy_fire_interval = 100;
        params->formation_margin_x = 6;
        params->player_shot_cooldown_ticks = 12;
        params->player_w = 22;
        params->player_h = 6;
        break;
    }
}

static void invaders_reset_particles(void)
{
    int i;

    for (i = 0; i < EXPLOSION_PARTICLE_MAX; ++i) {
        g_explosion_particles[i].active = 0;
    }
}

static void invaders_reset_formation(void)
{
    int row;
    int col;
    float formation_w = (INVADER_COLS * (INVADER_W + INVADER_SPACING_X)) - INVADER_SPACING_X;

    for (row = 0; row < INVADER_ROWS; ++row) {
        for (col = 0; col < INVADER_COLS; ++col) {
            g_enemies[row][col] = 1;
        }
    }

    g_invaders_alive = INVADER_ROWS * INVADER_COLS;
    g_form_x = (VIDEO_WIDTH - formation_w) * 0.5f;
    g_form_y = INVADER_START_Y;
    g_form_x_prev = g_form_x;
    g_form_y_prev = g_form_y;
    g_direction = 1;
    g_enemy_speed = g_params.enemy_speed;
    g_inv_anim_ticks = 0;
    g_inv_anim_frame = 0;
}

static int invaders_any_alive(void)
{
    return g_invaders_alive > 0;
}

static float invaders_bottom_y(void)
{
    int row;
    int col;

    for (row = INVADER_ROWS - 1; row >= 0; --row) {
        for (col = 0; col < INVADER_COLS; ++col) {
            if (g_enemies[row][col]) {
                return g_form_y + (row * (INVADER_H + INVADER_SPACING_Y)) + INVADER_H;
            }
        }
    }

    return g_form_y + (INVADER_ROWS * (INVADER_H + INVADER_SPACING_Y));
}

static int invaders_hit_enemy(float shot_x, float shot_y, float *out_x, float *out_y)
{
    int row;
    int col;

    for (row = 0; row < INVADER_ROWS; ++row) {
        for (col = 0; col < INVADER_COLS; ++col) {
            if (!g_enemies[row][col]) {
                continue;
            }

            {
                float ex = g_form_x + (col * (INVADER_W + INVADER_SPACING_X));
                float ey = g_form_y + (row * (INVADER_H + INVADER_SPACING_Y));

                if ((shot_x + PLAYER_SHOT_W) >= ex && shot_x <= (ex + INVADER_W) &&
                    (shot_y + PLAYER_SHOT_H) >= ey && shot_y <= (ey + INVADER_H)) {
                    g_enemies[row][col] = 0;
                    if (g_invaders_alive > 0) {
                        g_invaders_alive--;
                    }
                    if (out_x) {
                        *out_x = ex + (INVADER_W * 0.5f);
                    }
                    if (out_y) {
                        *out_y = ey + (INVADER_H * 0.5f);
                    }
                    return 1;
                }
            }
        }
    }

    return 0;
}

static int invaders_pick_shooter(int *out_x, int *out_y)
{
    int indices[INVADER_COLS][2];
    int targeted[INVADER_COLS];
    int count = 0;
    int targeted_count = 0;
    int row;
    int col;
    float player_center = g_player_x + (g_params.player_w * 0.5f);
    float targeting_range = 26.0f;

    for (col = 0; col < INVADER_COLS; ++col) {
        for (row = INVADER_ROWS - 1; row >= 0; --row) {
            if (g_enemies[row][col]) {
                indices[count][0] = row;
                indices[count][1] = col;
                count++;
                break;
            }
        }
    }

    if (count == 0) {
        return 0;
    }

    {
        for (col = 0; col < count; ++col) {
            int r = indices[col][0];
            int c = indices[col][1];
            float ex = g_form_x + (c * (INVADER_W + INVADER_SPACING_X));
            float center_x = ex + (INVADER_W * 0.5f);
            if (center_x >= (player_center - targeting_range) &&
                center_x <= (player_center + targeting_range)) {
                targeted[targeted_count] = col;
                targeted_count++;
            }
        }

        if (targeted_count > 0) {
            int pick = targeted[rand() % targeted_count];
            int r = indices[pick][0];
            int c = indices[pick][1];
            float ex = g_form_x + (c * (INVADER_W + INVADER_SPACING_X));
            float ey = g_form_y + (r * (INVADER_H + INVADER_SPACING_Y));
            *out_x = (int)(ex + (INVADER_W / 2));
            *out_y = (int)(ey + INVADER_H);
        } else {
            int pick = rand() % count;
            int r = indices[pick][0];
            int c = indices[pick][1];
            float ex = g_form_x + (c * (INVADER_W + INVADER_SPACING_X));
            float ey = g_form_y + (r * (INVADER_H + INVADER_SPACING_Y));
            *out_x = (int)(ex + (INVADER_W / 2));
            *out_y = (int)(ey + INVADER_H);
        }
    }

    return 1;
}

static void invaders_spawn_explosion(float x, float y)
{
    int i;
    int slot = 0;
    const float speed = 0.9f;
    const float dirs[8][2] = {
        { -1.0f, -1.0f },
        { 1.0f, -1.0f },
        { -1.0f, 1.0f },
        { 1.0f, 1.0f },
        { 0.0f, -1.0f },
        { 0.0f, 1.0f },
        { -1.0f, 0.0f },
        { 1.0f, 0.0f }
    };

    for (i = 0; i < 8; ++i) {
        for (; slot < EXPLOSION_PARTICLE_MAX; ++slot) {
            if (!g_explosion_particles[slot].active) {
                g_explosion_particles[slot].active = 1;
                g_explosion_particles[slot].x = x;
                g_explosion_particles[slot].y = y;
                g_explosion_particles[slot].vx = dirs[i][0] * speed;
                g_explosion_particles[slot].vy = dirs[i][1] * speed;
                g_explosion_particles[slot].life = EXPLOSION_PARTICLE_LIFE;
                slot++;
                break;
            }
        }
    }
}

static void invaders_update_particles(void)
{
    int i;

    for (i = 0; i < EXPLOSION_PARTICLE_MAX; ++i) {
        if (!g_explosion_particles[i].active) {
            continue;
        }

        g_explosion_particles[i].x += g_explosion_particles[i].vx;
        g_explosion_particles[i].y += g_explosion_particles[i].vy;
        g_explosion_particles[i].life--;
        if (g_explosion_particles[i].life <= 0) {
            g_explosion_particles[i].active = 0;
        }
    }
}

static int Invaders_GetAnimIntervalTicks(void)
{
    int alive = g_invaders_alive;
    int total = INVADER_ROWS * INVADER_COLS;

    if (alive <= 0) {
        return INVADER_ANIM_INTERVAL_BASE_TICKS;
    }

    {
        int interval = (INVADER_ANIM_INTERVAL_BASE_TICKS * alive) / total;
        if (interval < 6) {
            interval = 6;
        }
        return interval;
    }
}

void Invaders_Init(const GameSettings *settings)
{
    if (settings) {
        g_settings = *settings;
    } else {
        memset(&g_settings, 0, sizeof(g_settings));
    }

    invaders_select_params(g_settings.difficulty, &g_params);
    g_params.player_speed *= g_settings.speed_multiplier;
    g_params.player_shot_speed *= g_settings.speed_multiplier;
    g_params.enemy_speed *= g_settings.speed_multiplier;
    g_params.enemy_speed_max *= g_settings.speed_multiplier;
    g_params.enemy_accel *= g_settings.speed_multiplier;
    g_params.enemy_shot_speed *= g_settings.speed_multiplier;

    g_finished = 0;
    g_did_win = 0;
    g_elapsed_ticks = 0;
    g_target_ticks = g_params.target_seconds * INVADER_TICKS_PER_SECOND;
    g_player_x = (VIDEO_WIDTH - g_params.player_w) * 0.5f;
    g_player_x_prev = g_player_x;

    g_player_shot_active = 0;
    g_player_shot_cooldown = 0;
    g_fire_held = 0;
    g_enemy_shot_active = 0;
    g_score = 0;
    g_end_detail[0] = '\0';

    invaders_reset_formation();
    invaders_reset_particles();
    Invaders_StorePreviousState();

    g_use_keyboard = 1;
    g_sound_enabled = g_settings.sound_enabled ? 1 : 0;
    if (g_settings.input_mode == INPUT_JOYSTICK) {
        if (in_joystick_available()) {
            g_use_keyboard = 0;
        } else {
            g_use_keyboard = 1;
        }
    }

    while (in_keyhit()) {
        in_poll();
    }
}

void Invaders_StorePreviousState(void)
{
    g_form_x_prev = g_form_x;
    g_form_y_prev = g_form_y;
    g_player_x_prev = g_player_x;
    g_player_shot_x_prev = g_player_shot_x;
    g_player_shot_y_prev = g_player_shot_y;
    g_enemy_shot_x_prev = g_enemy_shot_x;
    g_enemy_shot_y_prev = g_enemy_shot_y;
}

void Invaders_Update(void)
{
    int move_dir = 0;
    int fire_down = 0;
    int fire_pressed = 0;

    if (g_finished) {
        return;
    }

    sound_update();

    g_inv_anim_ticks++;
    if (g_inv_anim_ticks >= Invaders_GetAnimIntervalTicks()) {
        g_inv_anim_ticks = 0;
        g_inv_anim_frame ^= 1;
    }

    if (g_use_keyboard) {
        if (kb_down(SC_LEFT)) {
            move_dir -= 1;
        }
        if (kb_down(SC_RIGHT)) {
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

    g_player_x += (float)move_dir * g_params.player_speed;
    g_player_x = clampf(g_player_x, 0.0f, (float)(VIDEO_WIDTH - g_params.player_w));

    if (g_player_shot_cooldown > 0) {
        g_player_shot_cooldown--;
    }

    if (fire_pressed && !g_player_shot_active && g_player_shot_cooldown == 0) {
        int player_y = invaders_player_y();

        g_player_shot_active = 1;
        g_player_shot_x = g_player_x + (g_params.player_w * 0.5f) - (PLAYER_SHOT_W * 0.5f);
        g_player_shot_y = player_y - PLAYER_SHOT_H;
        g_player_shot_x_prev = g_player_shot_x;
        g_player_shot_y_prev = g_player_shot_y;
        g_player_shot_cooldown = g_params.player_shot_cooldown_ticks;
        if (g_sound_enabled) {
            sound_play_tone(520, 30);
        }
    }

    if (g_player_shot_active) {
        g_player_shot_y -= g_params.player_shot_speed;
        if (g_player_shot_y < -PLAYER_SHOT_H) {
            g_player_shot_active = 0;
        } else {
            float hit_x = 0.0f;
            float hit_y = 0.0f;

            if (invaders_hit_enemy(g_player_shot_x, g_player_shot_y, &hit_x, &hit_y)) {
                g_player_shot_active = 0;
                invaders_spawn_explosion(hit_x, hit_y);
                g_score += 1000;
                if (g_sound_enabled) {
                    sound_play_tone(420, 35);
                }
            }
        }
    }

    if (!g_enemy_shot_active) {
        if (invaders_any_alive() && (rand() % g_params.enemy_fire_interval) == 0) {
            int sx = 0;
            int sy = 0;

            if (invaders_pick_shooter(&sx, &sy)) {
                g_enemy_shot_active = 1;
                g_enemy_shot_x = (float)sx;
                g_enemy_shot_y = (float)sy;
                g_enemy_shot_x_prev = g_enemy_shot_x;
                g_enemy_shot_y_prev = g_enemy_shot_y;
            }
        }
    }

    if (g_enemy_shot_active) {
        g_enemy_shot_y += g_params.enemy_shot_speed;
        if (g_enemy_shot_y > VIDEO_HEIGHT) {
            g_enemy_shot_active = 0;
        } else {
            float px = g_player_x;
            float py = (float)invaders_player_y();

            if ((g_enemy_shot_x + ENEMY_SHOT_W) >= px && g_enemy_shot_x <= (px + g_params.player_w) &&
                (g_enemy_shot_y + ENEMY_SHOT_H) >= py && g_enemy_shot_y <= (py + g_params.player_h)) {
                g_finished = 1;
                g_did_win = 0;
                if (g_sound_enabled) {
                    sound_play_tone(180, 120);
                }
                return;
            }
        }
    }

    if (!invaders_any_alive()) {
        invaders_reset_formation();
    }

    {
        float formation_w = (INVADER_COLS * (INVADER_W + INVADER_SPACING_X)) - INVADER_SPACING_X;
        float left_limit = (float)g_params.formation_margin_x;
        float right_limit = (float)(VIDEO_WIDTH - g_params.formation_margin_x) - formation_w;
        g_form_x += g_enemy_speed * (float)g_direction;

        if (g_direction > 0 && g_form_x >= right_limit) {
            g_form_x = right_limit;
            g_direction = -1;
            g_form_y += (float)g_params.step_down;
        } else if (g_direction < 0 && g_form_x <= left_limit) {
            g_form_x = left_limit;
            g_direction = 1;
            g_form_y += (float)g_params.step_down;
        }
    }

    g_enemy_speed += g_params.enemy_accel;
    if (g_enemy_speed > g_params.enemy_speed_max) {
        g_enemy_speed = g_params.enemy_speed_max;
    }

    invaders_update_particles();

    if (invaders_bottom_y() >= invaders_player_y()) {
        g_finished = 1;
        g_did_win = 0;
        if (g_sound_enabled) {
            sound_play_tone(160, 140);
        }
        return;
    }

    g_elapsed_ticks++;
    if (g_elapsed_ticks >= g_target_ticks) {
        g_finished = 1;
        g_did_win = 1;
        g_score += 50000;
        if (g_sound_enabled) {
            sound_play_tone(700, 120);
        }
    }
}

void Invaders_DrawInterpolated(float alpha)
{
    char hud[64];
    char timer[32];
    float form_x = g_form_x_prev + (g_form_x - g_form_x_prev) * alpha;
    float form_y = g_form_y_prev + (g_form_y - g_form_y_prev) * alpha;
    float player_x = g_player_x_prev + (g_player_x - g_player_x_prev) * alpha;
    float shot_x = g_player_shot_x_prev + (g_player_shot_x - g_player_shot_x_prev) * alpha;
    float shot_y = g_player_shot_y_prev + (g_player_shot_y - g_player_shot_y_prev) * alpha;
    float enemy_shot_x = g_enemy_shot_x_prev + (g_enemy_shot_x - g_enemy_shot_x_prev) * alpha;
    float enemy_shot_y = g_enemy_shot_y_prev + (g_enemy_shot_y - g_enemy_shot_y_prev) * alpha;
    int row;
    int col;
    int remaining = g_target_ticks - g_elapsed_ticks;
    int player_y = invaders_player_y();

    v_clear(0);

    for (row = 0; row < INVADER_ROWS; ++row) {
        for (col = 0; col < INVADER_COLS; ++col) {
            if (!g_enemies[row][col]) {
                continue;
            }
            {
                int x = (int)(form_x + (col * (INVADER_W + INVADER_SPACING_X)));
                int y = (int)(form_y + (row * (INVADER_H + INVADER_SPACING_Y)));
                int color_index = row;
                unsigned char color = INV_COLOR_ALIEN_ROW[0];
                const unsigned char *spr =
                    (g_inv_anim_frame == 0) ? INV_SPR_CRAB_A : INV_SPR_CRAB_B;
                if (color_index < 0) {
                    color_index = 0;
                } else if (color_index >= INVADER_ROWS) {
                    color_index = INVADER_ROWS - 1;
                }
                color = INV_COLOR_ALIEN_ROW[color_index];
                draw_mask_8x6(x, y, spr, color);
            }
        }
    }

    v_draw_dotted_rect((int)player_x, player_y, g_params.player_w, g_params.player_h,
                       INV_COLOR_PLAYER_SHIP_BASE, INV_COLOR_PLAYER_SHIP_DOTS, 1);
    draw_rect((int)player_x + (g_params.player_w / 2) - 1, player_y - 2, 2, 2, INV_COLOR_PLAYER_TURRET);

    if (g_player_shot_active) {
        draw_rect((int)shot_x, (int)shot_y, PLAYER_SHOT_W, PLAYER_SHOT_H, INV_COLOR_BULLET_PLAYER);
    }

    if (g_enemy_shot_active) {
        draw_rect((int)enemy_shot_x, (int)enemy_shot_y, ENEMY_SHOT_W, ENEMY_SHOT_H, INV_COLOR_BULLET_ENEMY);
    }

    for (row = 0; row < EXPLOSION_PARTICLE_MAX; ++row) {
        if (!g_explosion_particles[row].active) {
            continue;
        }
        {
            unsigned char color = INV_COLOR_EXPLOSION_0;
            if (g_explosion_particles[row].life < (EXPLOSION_PARTICLE_LIFE / 2)) {
                color = INV_COLOR_EXPLOSION_1;
            }
            draw_rect((int)g_explosion_particles[row].x, (int)g_explosion_particles[row].y, 2, 2, color);
        }
    }

    snprintf(hud, sizeof(hud), "D:%s x%.2f", difficulty_short(g_settings.difficulty), g_settings.speed_multiplier);
    v_puts(0, 0, "1978", INV_COLOR_HUD);
    v_puts(VIDEO_WIDTH - (text_len(hud) * 8), 0, hud, INV_COLOR_HUD);

    {
        char score[32];
        invaders_format_score(score, sizeof(score));
        draw_center_text(score, 4, 15);
    }

    if (remaining < 0) {
        remaining = 0;
    }
    snprintf(timer, sizeof(timer), "TIEMPO %d", remaining / INVADER_TICKS_PER_SECOND);
    v_puts(0, 10, timer, INV_COLOR_TIMER);

    v_present();
}

void Invaders_End(void)
{
    (void)g_did_win;
    (void)g_sound_enabled;
    invaders_format_score(g_end_detail, sizeof(g_end_detail));
}

int Invaders_IsFinished(void)
{
    return g_finished;
}

int Invaders_DidWin(void)
{
    return g_did_win;
}

const char *Invaders_GetEndDetail(void)
{
    return g_end_detail;
}

uint64_t Invaders_GetScore(void)
{
    return (uint64_t)g_score;
}
