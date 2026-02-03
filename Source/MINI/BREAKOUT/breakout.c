#include "breakout.h"

#include "../../CORE/input.h"
#include "../../CORE/options.h"
#include "../../CORE/sound.h"
#include "../../CORE/video.h"
#include "../../CORE/keyboard.h"
#include "../../CORE/high_scores.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PLAY_LEFT 8
#define PLAY_RIGHT 312
#define PLAY_TOP 16
#define PLAY_BOTTOM 192

#define PADDLE_Y 182
#define PADDLE_H 6

#define BALL_SIZE 2

#define BRICK_COLS 12
#define BRICK_ROWS 6
#define BRICK_W 20
#define BRICK_H 6
#define BRICK_GAP 2

#define BRICK_AREA_WIDTH ((BRICK_COLS * BRICK_W) + ((BRICK_COLS - 1) * BRICK_GAP))
#define BRICK_AREA_HEIGHT ((BRICK_ROWS * BRICK_H) + ((BRICK_ROWS - 1) * BRICK_GAP))
#define BRICK_LEFT (PLAY_LEFT + ((PLAY_RIGHT - PLAY_LEFT - BRICK_AREA_WIDTH) / 2))

#define BALL_MIN_VX_SCALE 0.35f
#define BRK_START_LIVES 3
#define BRK_LIFE_SCORE_BONUS 10000
#define BRK_RESET_PAUSE_MS 450
#define BRK_TICKS_PER_SECOND 60
#define BRK_RESET_PAUSE_TICKS ((BRK_RESET_PAUSE_MS * BRK_TICKS_PER_SECOND + 999) / 1000)
#define BRK_LAUNCH_TIMEOUT_SECONDS 10
#define BRK_LAUNCH_TIMEOUT_TICKS (BRK_LAUNCH_TIMEOUT_SECONDS * BRK_TICKS_PER_SECOND)
#define BRK_DIFFICULTY_SPEED_EASY 0.40f
#define BRK_DIFFICULTY_SPEED_NORMAL 0.50f
#define BRK_DIFFICULTY_SPEED_HARD 0.65f

typedef struct {
    float paddle_speed;
    float ball_speed_x;
    float ball_speed_y;
    int paddle_w;
    int hits_per_drop;
    int wall_start_row_offset;
    int score_brick;
    int score_drop_bonus;
} BrkParams;

typedef struct {
    unsigned char alive[BRICK_ROWS][BRICK_COLS];
    int top_y;
    int drop_count;
} BrickGrid;

static GameSettings g_settings;
static BrkParams g_params;
static BrickGrid g_grid;

static int g_finished = 0;
static int g_did_win = 0;
static int g_sound_enabled = 0;
static int g_use_keyboard = 1;

static float g_paddle_x = 0.0f;
static float g_paddle_x_prev = 0.0f;

static float g_ball_x = 0.0f;
static float g_ball_y = 0.0f;
static float g_ball_x_prev = 0.0f;
static float g_ball_y_prev = 0.0f;
static float g_ball_vx = 0.0f;
static float g_ball_vy = 0.0f;
static int g_ball_attached = 0;
static int g_ball_launch_ticks = 0;

static int g_hits_since_drop = 0;
static uint64_t g_score = 0;
static int g_bricks_remaining = 0;
static int g_lives = 0;
static int g_reset_delay_ticks = 0;
static char g_end_detail[32] = "";

static const unsigned char g_brick_colors[BRICK_ROWS] = {87, 86, 85, 84, 83, 82};

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

static float absf(float value)
{
    return value < 0.0f ? -value : value;
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

static void draw_center_text(const char *text, int y, unsigned char color)
{
    int len = 0;
    int x;

    if (!text) {
        return;
    }

    len = text_len(text);
    x = (VIDEO_WIDTH - (len * 8)) / 2;
    v_puts(x, y, text, color);
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

static void breakout_select_params(unsigned char difficulty, BrkParams *params)
{
    if (!params) {
        return;
    }

    switch (difficulty) {
    case DIFFICULTY_EASY:
        params->paddle_speed = 3.0f;
        params->ball_speed_x = 1.6f;
        params->ball_speed_y = 1.7f;
        params->paddle_w = 48;
        params->hits_per_drop = 6;
        params->wall_start_row_offset = 0;
        params->score_brick = 120;
        params->score_drop_bonus = 250;
        break;
    case DIFFICULTY_HARD:
        params->paddle_speed = 3.6f;
        params->ball_speed_x = 2.2f;
        params->ball_speed_y = 2.3f;
        params->paddle_w = 34;
        params->hits_per_drop = 4;
        params->wall_start_row_offset = 0;
        params->score_brick = 160;
        params->score_drop_bonus = 350;
        break;
    case DIFFICULTY_NORMAL:
    default:
        params->paddle_speed = 3.3f;
        params->ball_speed_x = 1.9f;
        params->ball_speed_y = 2.0f;
        params->paddle_w = 40;
        params->hits_per_drop = 5;
        params->wall_start_row_offset = 0;
        params->score_brick = 140;
        params->score_drop_bonus = 300;
        break;
    }
}

static float breakout_difficulty_speed_scale(unsigned char difficulty)
{
    switch (difficulty) {
    case DIFFICULTY_EASY:
        return BRK_DIFFICULTY_SPEED_EASY;
    case DIFFICULTY_HARD:
        return BRK_DIFFICULTY_SPEED_HARD;
    case DIFFICULTY_NORMAL:
    default:
        return BRK_DIFFICULTY_SPEED_NORMAL;
    }
}

static void breakout_prepare_ball(void)
{
    g_ball_attached = 1;
    g_ball_launch_ticks = BRK_LAUNCH_TIMEOUT_TICKS;
    g_ball_x = g_paddle_x + (g_params.paddle_w * 0.5f) - (BALL_SIZE * 0.5f);
    g_ball_y = PADDLE_Y - BALL_SIZE - 2;
    g_ball_vx = 0.0f;
    g_ball_vy = 0.0f;
}

static void breakout_launch_ball(void)
{
    int dir = (rand() % 2) ? 1 : -1;
    g_ball_attached = 0;
    g_ball_vx = g_params.ball_speed_x * (float)dir;
    g_ball_vy = -g_params.ball_speed_y;
    if (absf(g_ball_vx) < 0.1f) {
        g_ball_vx = 0.1f * (float)dir;
    }
}

static void breakout_reset_positions(void)
{
    g_paddle_x = (PLAY_LEFT + PLAY_RIGHT - g_params.paddle_w) * 0.5f;
    g_paddle_x_prev = g_paddle_x;
    breakout_prepare_ball();
    g_ball_x_prev = g_ball_x;
    g_ball_y_prev = g_ball_y;
}

static void breakout_drop_wall(void)
{
    g_grid.top_y += (BRICK_H + BRICK_GAP);
    g_grid.drop_count++;
    if (g_params.score_drop_bonus > 0) {
        g_score += (uint64_t)g_params.score_drop_bonus;
    }
    if (g_sound_enabled) {
        sound_play_tone(240, 90);
    }
}

void Breakout_Init(const GameSettings *settings)
{
    int row;
    int col;
    float difficulty_speed_scale;

    if (!settings) {
        return;
    }

    g_settings = *settings;
    breakout_select_params(g_settings.difficulty, &g_params);
    difficulty_speed_scale = breakout_difficulty_speed_scale(g_settings.difficulty);
    g_params.paddle_speed *= difficulty_speed_scale;
    g_params.ball_speed_x *= difficulty_speed_scale;
    g_params.ball_speed_y *= difficulty_speed_scale;
    g_params.paddle_speed *= g_settings.speed_multiplier;
    g_params.ball_speed_x *= g_settings.speed_multiplier;
    g_params.ball_speed_y *= g_settings.speed_multiplier;

    g_finished = 0;
    g_did_win = 0;
    g_hits_since_drop = 0;
    g_score = 0;
    g_lives = BRK_START_LIVES;
    g_reset_delay_ticks = 0;
    g_end_detail[0] = '\0';

    g_grid.top_y = PLAY_TOP + g_params.wall_start_row_offset;
    g_grid.drop_count = 0;
    g_bricks_remaining = BRICK_ROWS * BRICK_COLS;

    for (row = 0; row < BRICK_ROWS; ++row) {
        for (col = 0; col < BRICK_COLS; ++col) {
            g_grid.alive[row][col] = 1;
        }
    }

    breakout_reset_positions();

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

void Breakout_StorePreviousState(void)
{
    g_paddle_x_prev = g_paddle_x;
    g_ball_x_prev = g_ball_x;
    g_ball_y_prev = g_ball_y;
}

void Breakout_Update(void)
{
    float paddle_speed = g_params.paddle_speed;
    float ball_next_x;
    float ball_next_y;

    if (g_finished) {
        return;
    }

    sound_update();

    if (g_reset_delay_ticks > 0) {
        g_reset_delay_ticks--;
        if (g_reset_delay_ticks == 0) {
            breakout_reset_positions();
        }
        return;
    }

    if (g_use_keyboard) {
        int move_dir = 0;

        if (kb_down(SC_LEFT)) {
            move_dir -= 1;
        }
        if (kb_down(SC_RIGHT)) {
            move_dir += 1;
        }

        g_paddle_x += (float)move_dir * paddle_speed;
    } else {
        int dx = 0;
        int dy = 0;

        if (in_joystick_direction(&dx, &dy, NULL)) {
            g_paddle_x += (float)dx * paddle_speed;
        }
    }

    g_paddle_x = clampf(g_paddle_x, (float)PLAY_LEFT, (float)(PLAY_RIGHT - g_params.paddle_w));

    if (g_ball_attached) {
        int launch_now = 0;

        g_ball_x = g_paddle_x + (g_params.paddle_w * 0.5f) - (BALL_SIZE * 0.5f);
        g_ball_y = PADDLE_Y - BALL_SIZE - 2;

        if (g_ball_launch_ticks > 0) {
            g_ball_launch_ticks--;
        }

        if (g_use_keyboard && kb_down(SC_SPACE)) {
            launch_now = 1;
        }

        if (g_ball_launch_ticks <= 0) {
            launch_now = 1;
        }

        if (launch_now) {
            breakout_launch_ball();
        } else {
            return;
        }
    }

    ball_next_x = g_ball_x + g_ball_vx;
    ball_next_y = g_ball_y + g_ball_vy;

    if (ball_next_x <= PLAY_LEFT) {
        ball_next_x = (float)PLAY_LEFT;
        g_ball_vx = -g_ball_vx;
        if (g_sound_enabled) {
            sound_play_tone(460, 20);
        }
    } else if ((ball_next_x + BALL_SIZE) >= PLAY_RIGHT) {
        ball_next_x = (float)(PLAY_RIGHT - BALL_SIZE);
        g_ball_vx = -g_ball_vx;
        if (g_sound_enabled) {
            sound_play_tone(460, 20);
        }
    }

    if (ball_next_y <= PLAY_TOP) {
        ball_next_y = (float)PLAY_TOP;
        g_ball_vy = -g_ball_vy;
        if (g_sound_enabled) {
            sound_play_tone(500, 20);
        }
    }

    if (ball_next_y > PLAY_BOTTOM) {
        g_lives--;
        if (g_sound_enabled) {
            sound_play_tone(180, 120);
        }
        if (g_lives <= 0) {
            g_finished = 1;
            g_did_win = 0;
            return;
        }
        g_reset_delay_ticks = BRK_RESET_PAUSE_TICKS;
        return;
    }

    {
        float paddle_right = g_paddle_x + g_params.paddle_w;
        float paddle_top = (float)PADDLE_Y;
        float paddle_bottom = (float)(PADDLE_Y + PADDLE_H);

        if (g_ball_vy > 0.0f &&
            (ball_next_x + BALL_SIZE) >= g_paddle_x &&
            ball_next_x <= paddle_right &&
            (ball_next_y + BALL_SIZE) >= paddle_top &&
            ball_next_y <= paddle_bottom) {
            float paddle_center = g_paddle_x + (g_params.paddle_w * 0.5f);
            float ball_center = ball_next_x + (BALL_SIZE * 0.5f);
            float rel = (ball_center - paddle_center) / (g_params.paddle_w * 0.5f);
            float base_vx = g_params.ball_speed_x;
            float min_vx = absf(base_vx) * BALL_MIN_VX_SCALE;
            float new_vx;
            float magnitude;
            float sign;

            if (rel < -1.0f) {
                rel = -1.0f;
            } else if (rel > 1.0f) {
                rel = 1.0f;
            }

            magnitude = absf(base_vx) * (0.4f + (0.6f * absf(rel)));
            sign = (rel < -0.2f) ? -1.0f : (rel > 0.2f) ? 1.0f : (g_ball_vx < 0.0f ? -1.0f : 1.0f);
            if (magnitude < min_vx) {
                magnitude = min_vx;
            }
            new_vx = magnitude * sign;

            g_ball_vx = new_vx;
            g_ball_vy = -absf(g_ball_vy);
            ball_next_y = paddle_top - BALL_SIZE;

            g_hits_since_drop++;
            if (g_hits_since_drop >= g_params.hits_per_drop) {
                g_hits_since_drop = 0;
                breakout_drop_wall();
            }

            if (g_sound_enabled) {
                sound_play_tone(520, 30);
            }
        }
    }

    {
        float ball_center_x = ball_next_x + (BALL_SIZE * 0.5f);
        float ball_center_y = ball_next_y + (BALL_SIZE * 0.5f);
        float grid_left = (float)BRICK_LEFT;
        float grid_right = grid_left + BRICK_AREA_WIDTH;
        float grid_top = (float)g_grid.top_y;
        float grid_bottom = grid_top + BRICK_AREA_HEIGHT;

        if (ball_center_x >= grid_left && ball_center_x < grid_right &&
            ball_center_y >= grid_top && ball_center_y < grid_bottom) {
            int local_x = (int)(ball_center_x - grid_left);
            int local_y = (int)(ball_center_y - grid_top);
            int cell_w = BRICK_W + BRICK_GAP;
            int cell_h = BRICK_H + BRICK_GAP;
            int col = local_x / cell_w;
            int row = local_y / cell_h;
            int in_brick_x = local_x % cell_w;
            int in_brick_y = local_y % cell_h;

            if (col >= 0 && col < BRICK_COLS && row >= 0 && row < BRICK_ROWS) {
                if (in_brick_x < BRICK_W && in_brick_y < BRICK_H && g_grid.alive[row][col]) {
                    g_grid.alive[row][col] = 0;
                    g_bricks_remaining--;
                    g_score += (uint64_t)g_params.score_brick;
                    g_ball_vy = -g_ball_vy;
                    if (g_sound_enabled) {
                        sound_play_tone(420, 35);
                    }
                    if (g_bricks_remaining <= 0) {
                        g_finished = 1;
                        g_did_win = 1;
                        if (g_sound_enabled) {
                            sound_play_tone(700, 120);
                        }
                    }
                }
            }
        }
    }

    if ((g_grid.top_y + BRICK_AREA_HEIGHT) >= PADDLE_Y) {
        g_finished = 1;
        g_did_win = 0;
        if (g_sound_enabled) {
            sound_play_tone(180, 120);
        }
        return;
    }

    g_ball_x = ball_next_x;
    g_ball_y = ball_next_y;
}

void Breakout_DrawInterpolated(float alpha)
{
    char hud[64];
    char score_text[32];
    float clamped_alpha = clampf(alpha, 0.0f, 1.0f);
    float paddle_x = g_paddle_x_prev + (g_paddle_x - g_paddle_x_prev) * clamped_alpha;
    float ball_x = g_ball_x_prev + (g_ball_x - g_ball_x_prev) * clamped_alpha;
    float ball_y = g_ball_y_prev + (g_ball_y - g_ball_y_prev) * clamped_alpha;
    int row;
    int col;
    v_clear(0);

    for (row = 0; row < BRICK_ROWS; ++row) {
        for (col = 0; col < BRICK_COLS; ++col) {
            if (!g_grid.alive[row][col]) {
                continue;
            }
            {
                int x = BRICK_LEFT + (col * (BRICK_W + BRICK_GAP));
                int y = g_grid.top_y + (row * (BRICK_H + BRICK_GAP));
                unsigned char color = g_brick_colors[row % BRICK_ROWS];
                v_fill_rect(x, y, BRICK_W, BRICK_H, color);
            }
        }
    }

    v_fill_rect((int)ball_x, (int)ball_y, BALL_SIZE, BALL_SIZE, 15);

    v_draw_dotted_rect((int)paddle_x, PADDLE_Y, g_params.paddle_w, PADDLE_H,
                       BRK_COLOR_PLAYER_SHIP_BASE, BRK_COLOR_PLAYER_SHIP_DOTS, 1);

    snprintf(hud, sizeof(hud), "D:%s x%.2f", difficulty_short(g_settings.difficulty), g_settings.speed_multiplier);
    v_puts(0, 0, "1979", BRK_COLOR_HUD);
    v_puts(VIDEO_WIDTH - (text_len(hud) * 8), 0, hud, BRK_COLOR_HUD);

    high_scores_format_score(score_text, sizeof(score_text), g_score);
    {
        int score_x = (VIDEO_WIDTH - (text_len(score_text) * 8)) / 2;
        if (score_x < 0) {
            score_x = 0;
        }
        v_puts(score_x, 0, score_text, BRK_COLOR_HUD);
    }

    snprintf(hud, sizeof(hud), "VIDAS %d", g_lives);
    v_puts(VIDEO_WIDTH - (text_len(hud) * 8), VIDEO_HEIGHT - 8, hud, BRK_COLOR_HUD);

    v_present();
}

void Breakout_End(void)
{
    char score_text[16];

    if (g_lives > 0) {
        g_score += (uint64_t)g_lives * BRK_LIFE_SCORE_BONUS;
    }
    high_scores_format_score(score_text, sizeof(score_text), g_score);
    snprintf(g_end_detail, sizeof(g_end_detail), "PUNTOS %s", score_text);
}

int Breakout_IsFinished(void)
{
    return g_finished;
}

int Breakout_DidWin(void)
{
    return g_did_win;
}

const char *Breakout_GetEndDetail(void)
{
    return g_end_detail;
}

uint64_t Breakout_GetScore(void)
{
    return g_score;
}
