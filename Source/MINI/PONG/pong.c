#include "pong.h"

#include "../../CORE/input.h"
#include "../../CORE/options.h"
#include "../../CORE/sound.h"
#include "../../CORE/timer.h"
#include "../../CORE/video.h"
#include "../../CORE/keyboard.h"
#include "../../CORE/high_scores.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PONG_PADDLE_W 6
#define PONG_BALL_SIZE 2
#define PONG_LEFT_X 16
#define PONG_RIGHT_X (VIDEO_WIDTH - 16 - PONG_PADDLE_W)
#define PONG_WIN_TARGET_DEFAULT 3
#define PONG_SPEED_SCALE 0.50f
#define PONG_INITIAL_VY_SCALE 0.25f
#define PONG_SERVE_DELAY_TICKS 80
#define PONG_TIME_TARGET_MS 60000UL
#define PONG_TIME_BONUS_MAX 20000UL
typedef struct {
    float paddle_speed;
    float ball_speed_x;
    float ball_speed_y;
    float cpu_step;
    int paddle_h;
    float cpu_error;
    float cpu_deadzone;
    float cpu_react_delay;
} PongParams;

static GameSettings g_settings;
static PongParams g_params;
static int g_finished = 0;
static int g_did_win = 0;
static int g_player_score = 0;
static int g_cpu_score = 0;
static int g_win_target = PONG_WIN_TARGET_DEFAULT;
static float g_player_y = 0.0f;
static float g_cpu_y = 0.0f;
static float g_ball_x = 0.0f;
static float g_ball_y = 0.0f;
static float g_player_y_prev = 0.0f;
static float g_cpu_y_prev = 0.0f;
static float g_ball_x_prev = 0.0f;
static float g_ball_y_prev = 0.0f;
static float g_ball_vx = 0.0f;
static float g_ball_vy = 0.0f;
static float g_ball_base_vx = 0.0f;
static float g_ball_base_vy = 0.0f;
static int g_use_keyboard = 1;
static int g_last_scorer = 0;
static float g_cpu_react_cd = 0.0f;
static int g_initial_serve = 0;
static int g_serve_delay_ticks = 0;
static int g_sound_enabled = 0;
static char g_end_detail[32] = "";
static uint32_t g_start_time_us = 0;
static uint32_t g_finish_time_us = 0;
static uint64_t g_final_score = 0;

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

static void draw_center_text(const char *text, int y, unsigned char color)
{
    int len = 0;
    int x;

    while (text[len] != '\0') {
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

static void pong_select_params(unsigned char difficulty, PongParams *params)
{
    if (!params) {
        return;
    }

    switch (difficulty) {
    case DIFFICULTY_EASY:
        params->paddle_speed = 3.5f;
        params->ball_speed_x = 2.5f;
        params->ball_speed_y = 1.3f;
        params->cpu_step = 2.0f;
        params->paddle_h = 24;
        params->cpu_error = 6.0f;
        params->cpu_deadzone = 4.0f;
        params->cpu_react_delay = 2.2f;
        break;
    case DIFFICULTY_HARD:
        params->paddle_speed = 4.0f;
        params->ball_speed_x = 3.0f;
        params->ball_speed_y = 1.6f;
        params->cpu_step = 2.55f;       // Ajuste de dificultad
        params->paddle_h = 20;
        params->cpu_error = 2.8f;       // Ajuste de dificultad
        params->cpu_deadzone = 1.2f;    // Ajuste de dificultad
        params->cpu_react_delay = 0.95f; // Ajuste de dificultad
        break;
    case DIFFICULTY_NORMAL:
    default:
        params->paddle_speed = 3.0f;
        params->ball_speed_x = 2.2f;
        params->ball_speed_y = 1.4f;
        params->cpu_step = 2.35f;        // Ajuste de dificultad
        params->paddle_h = 22;
        params->cpu_error = 4.6f;        // Ajuste de dificultad
        params->cpu_deadzone = 2.5f;     // Ajuste de dificultad
        params->cpu_react_delay = 1.15f; // Ajuste de dificultad
        break;
    }
}

static void pong_reset_ball(int scorer)
{
    float vy_scale = 0;
    if (g_initial_serve) {
        // Variación suave estilo Pong original
        float jitter = ((float)(rand() % 21) - 10.0f) / 100.0f; 
        // Variación en [-0.10, +0.10]

        vy_scale = PONG_INITIAL_VY_SCALE * (1.0f + jitter);

        // OJO: evita tiro vertical o muerto
        if (vy_scale < 0.18f) vy_scale = 0.18f;
        if (vy_scale > 0.32f) vy_scale = 0.32f;
    } else {
        vy_scale = 0.85f + ((float)(rand() % 31) / 100.0f); // Rango 0.85–1.15
    }
    g_ball_x = (VIDEO_WIDTH - PONG_BALL_SIZE) * 0.5f;
    g_ball_y = (VIDEO_HEIGHT - PONG_BALL_SIZE) * 0.5f;

    if (scorer > 0) {
        g_ball_vx = -g_ball_base_vx;
    } else {
        g_ball_vx = g_ball_base_vx;
    }

    if (g_last_scorer >= 0) {
        g_ball_vy = g_ball_base_vy * vy_scale;
    } else {
        g_ball_vy = -g_ball_base_vy * vy_scale;
    }

    g_last_scorer = scorer;
    g_initial_serve = 1;
    if (scorer != 0) {
        g_serve_delay_ticks = PONG_SERVE_DELAY_TICKS;
    }
}

static void pong_draw_rect(int x, int y, int w, int h, unsigned char color)
{
    int ix;
    int iy;

    for (iy = 0; iy < h; ++iy) {
        for (ix = 0; ix < w; ++ix) {
            v_putpixel(x + ix, y + iy, color);
        }
    }
}

static void pong_draw_player_paddle(int x, int y, int w, int h)
{
    v_draw_dotted_rect(x, y, w, h, 18, 15, 0);
}

static uint64_t pong_calculate_score(void)
{
    uint64_t base_score;
    uint64_t bonus = 0;
    uint32_t elapsed_ms;
    uint32_t target_ms = PONG_TIME_TARGET_MS;

    if (g_cpu_score <= 0) {
        base_score = 50000;
    } else if (g_cpu_score == 1) {
        base_score = 25000;
    } else {
        base_score = 10000;
    }

    if (g_finish_time_us > g_start_time_us) {
        elapsed_ms = (g_finish_time_us - g_start_time_us) / 1000U;
    } else {
        elapsed_ms = 0;
    }

    if (elapsed_ms < target_ms) {
        bonus = (uint64_t)(target_ms - elapsed_ms) * PONG_TIME_BONUS_MAX / target_ms;
    }

    if (bonus > PONG_TIME_BONUS_MAX) {
        bonus = PONG_TIME_BONUS_MAX;
    }
    if (bonus > base_score) {
        bonus = base_score;
    }

    return base_score + bonus;
}

void Pong_Init(const GameSettings *settings)
{
    if (settings) {
        g_settings = *settings;
    } else {
        memset(&g_settings, 0, sizeof(g_settings));
    }

    pong_select_params(g_settings.difficulty, &g_params);
    g_params.paddle_speed *= PONG_SPEED_SCALE;
    g_params.ball_speed_x *= PONG_SPEED_SCALE;
    g_params.ball_speed_y *= PONG_SPEED_SCALE;
    g_params.cpu_step *= PONG_SPEED_SCALE;
    g_params.paddle_speed *= g_settings.speed_multiplier;
    g_params.cpu_step *= g_settings.speed_multiplier;
    g_ball_base_vx = g_params.ball_speed_x * g_settings.speed_multiplier;
    g_ball_base_vy = g_params.ball_speed_y * g_settings.speed_multiplier;

    g_player_score = 0;
    g_cpu_score = 0;
    g_finished = 0;
    g_did_win = 0;
    g_last_scorer = 0;
    g_cpu_react_cd = 0.0f;
    g_initial_serve = 1;
    g_serve_delay_ticks = 0;
    g_start_time_us = timer_now_us();
    g_finish_time_us = g_start_time_us;
    g_final_score = 0;

    g_player_y = (VIDEO_HEIGHT - g_params.paddle_h) * 0.5f;
    g_cpu_y = g_player_y;
    g_player_y_prev = g_player_y;
    g_cpu_y_prev = g_cpu_y;

    pong_reset_ball(0);
    Pong_StorePreviousState();
    g_use_keyboard = 1;
    g_sound_enabled = g_settings.sound_enabled ? 1 : 0;
    g_end_detail[0] = '\0';
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

void Pong_StorePreviousState(void)
{
    g_player_y_prev = g_player_y;
    g_cpu_y_prev = g_cpu_y;
    g_ball_x_prev = g_ball_x;
    g_ball_y_prev = g_ball_y;
}

void Pong_Update(void)
{
    float ball_center_y;
    float paddle_center_y;
    float impact;
    float target_y;
    float delta;

    if (g_finished) {
        return;
    }

    sound_update();

    if (g_use_keyboard) {
        int move_dir = 0;

        if (kb_down(SC_UP))   move_dir -= 1;
        if (kb_down(SC_DOWN)) move_dir += 1;

        g_player_y += (float)move_dir * g_params.paddle_speed;
    } else {
        int dx = 0;
        int dy = 0;

        if (in_joystick_direction(&dx, &dy, NULL)) {
            g_player_y += (float)dy * g_params.paddle_speed;
        }
    }

    g_player_y = clampf(g_player_y, 0.0f, (float)(VIDEO_HEIGHT - g_params.paddle_h));

    {
        int should_move = 1;

        // Enfriamiento de reacción de CPU con fracciones
        {
            float delay = g_params.cpu_react_delay;

            // Si la bola se aleja, reacciona más lento
            if (g_ball_vx < 0.0f && delay > 0.0f) {
                delay *= 1.6f;
            }

            if (delay > 0.0f) {
                if (g_cpu_react_cd > 0.0f) {
                    g_cpu_react_cd -= 1.0f;
                    should_move = 0;
                } else {
                    g_cpu_react_cd = delay;
                }
            }
        }

        if (g_settings.difficulty == DIFFICULTY_HARD) {
            target_y = g_ball_y - (g_params.paddle_h * 0.5f) + g_params.cpu_error;
            if (should_move && absf(target_y - g_cpu_y) > g_params.cpu_deadzone) {
                delta = clampf(target_y - g_cpu_y, -g_params.cpu_step, g_params.cpu_step);
                g_cpu_y += delta;
            }
        } else {
            float ball_center = g_ball_y + (PONG_BALL_SIZE * 0.5f);
            float paddle_center = g_cpu_y + (g_params.paddle_h * 0.5f);
            float target_center = ball_center + g_params.cpu_error;

            if (g_ball_vx < 0.0f) {
                target_center = VIDEO_HEIGHT * 0.5f;
            }

            if (should_move && absf(target_center - paddle_center) > g_params.cpu_deadzone) {
                target_y = target_center - (g_params.paddle_h * 0.5f);
                delta = clampf(target_y - g_cpu_y, -g_params.cpu_step, g_params.cpu_step);
                g_cpu_y += delta;
            }
        }
    }

    g_cpu_y = clampf(g_cpu_y, 0.0f, (float)(VIDEO_HEIGHT - g_params.paddle_h));

    if (g_serve_delay_ticks > 0) {
        g_serve_delay_ticks--;
        return;
    }

    g_ball_x += g_ball_vx;
    g_ball_y += g_ball_vy;

    if (g_ball_y <= 0.0f) {
        g_ball_y = 0.0f;
        g_ball_vy = -g_ball_vy;
        if (g_sound_enabled) {
            sound_play_tone(320, 35);
        }
    } else if (g_ball_y >= (VIDEO_HEIGHT - PONG_BALL_SIZE)) {
        g_ball_y = (float)(VIDEO_HEIGHT - PONG_BALL_SIZE);
        g_ball_vy = -g_ball_vy;
        if (g_sound_enabled) {
            sound_play_tone(320, 35);
        }
    }

    if (g_ball_vx < 0.0f &&
        g_ball_x <= (PONG_LEFT_X + PONG_PADDLE_W) &&
        (g_ball_x + PONG_BALL_SIZE) >= PONG_LEFT_X) {
        int paddle_y = (int)g_player_y;
        int ball_y = (int)g_ball_y;

        if ((ball_y + PONG_BALL_SIZE - 1) >= paddle_y &&
            ball_y <= (paddle_y + g_params.paddle_h - 1)) {
            g_ball_x = (float)(PONG_LEFT_X + PONG_PADDLE_W + 1);
            g_ball_vx = -g_ball_vx;
            if (g_sound_enabled) {
                sound_play_tone(440, 45);
            }

            ball_center_y = g_ball_y + (PONG_BALL_SIZE * 0.5f);
            paddle_center_y = g_player_y + (g_params.paddle_h * 0.5f);
            impact = (ball_center_y - paddle_center_y) / (g_params.paddle_h * 0.5f);
            impact = clampf(impact, -1.0f, 1.0f);
            g_ball_vy = impact * g_ball_base_vy;
        }
    }

    if (g_ball_vx > 0.0f &&
        (g_ball_x + PONG_BALL_SIZE) >= PONG_RIGHT_X &&
        g_ball_x <= (PONG_RIGHT_X + PONG_PADDLE_W)) {
        int paddle_y = (int)g_cpu_y;
        int ball_y = (int)g_ball_y;

        if ((ball_y + PONG_BALL_SIZE - 1) >= paddle_y &&
            ball_y <= (paddle_y + g_params.paddle_h - 1)) {
            g_ball_x = (float)(PONG_RIGHT_X - PONG_BALL_SIZE - 1);
            g_ball_vx = -g_ball_vx;
            if (g_sound_enabled) {
                sound_play_tone(380, 45);
            }

            ball_center_y = g_ball_y + (PONG_BALL_SIZE * 0.5f);
            paddle_center_y = g_cpu_y + (g_params.paddle_h * 0.5f);
            impact = (ball_center_y - paddle_center_y) / (g_params.paddle_h * 0.5f);
            impact = clampf(impact, -1.0f, 1.0f);
            g_ball_vy = impact * g_ball_base_vy;
        }
    }

    if (g_ball_x < -PONG_BALL_SIZE) {
        g_cpu_score++;
        if (g_sound_enabled) {
            sound_play_tone(260, 80);
        }
        if (g_cpu_score >= g_win_target) {
            g_finished = 1;
            g_did_win = 0;
            g_finish_time_us = timer_now_us();
            return;
        }
        pong_reset_ball(-1);
    } else if (g_ball_x > VIDEO_WIDTH) {
        g_player_score++;
        if (g_sound_enabled) {
            sound_play_tone(520, 80);
        }
        if (g_player_score >= g_win_target) {
            g_finished = 1;
            g_did_win = 1;
            g_finish_time_us = timer_now_us();
            return;
        }
        pong_reset_ball(1);
    }
}

void Pong_DrawInterpolated(float alpha)
{
    char hud[64];
    char score[32];
    float player_y = g_player_y_prev + (g_player_y - g_player_y_prev) * alpha;
    float cpu_y = g_cpu_y_prev + (g_cpu_y - g_cpu_y_prev) * alpha;
    float ball_x = g_ball_x_prev + (g_ball_x - g_ball_x_prev) * alpha;
    float ball_y = g_ball_y_prev + (g_ball_y - g_ball_y_prev) * alpha;

    v_clear(0);

    pong_draw_player_paddle(PONG_LEFT_X, (int)player_y, PONG_PADDLE_W, g_params.paddle_h);
    pong_draw_rect(PONG_RIGHT_X, (int)cpu_y, PONG_PADDLE_W, g_params.paddle_h, 15);
    pong_draw_rect((int)ball_x, (int)ball_y, PONG_BALL_SIZE, PONG_BALL_SIZE, 12);

    snprintf(hud, sizeof(hud), "D:%s x%.2f", difficulty_short(g_settings.difficulty), g_settings.speed_multiplier);
    v_puts(0, 0, "1972", 7);
    v_puts(VIDEO_WIDTH - (text_len(hud) * 8), 0, hud, 7);

    snprintf(score, sizeof(score), "P1 %d  CPU %d", g_player_score, g_cpu_score);
    draw_center_text(score, 4, 15);

    v_present();
}

void Pong_End(void)
{
    if (g_did_win) {
        char score_text[16];
        g_final_score = pong_calculate_score();
        high_scores_format_score(score_text, sizeof(score_text), g_final_score);
        snprintf(g_end_detail, sizeof(g_end_detail), "PUNTOS %s", score_text);
    } else {
        snprintf(g_end_detail, sizeof(g_end_detail), "P1 %d - CPU %d", g_player_score, g_cpu_score);
    }
}

int Pong_IsFinished(void)
{
    return g_finished;
}

int Pong_DidWin(void)
{
    return g_did_win;
}

const char *Pong_GetEndDetail(void)
{
    return g_end_detail;
}

uint64_t Pong_GetScore(void)
{
    return g_final_score;
}
