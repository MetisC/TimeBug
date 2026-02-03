#include "year_launcher.h"

#include "../CORE/input.h"
#include "../CORE/keyboard.h"
#include "../CORE/options.h"
#include "../CORE/text.h"
#include "../CORE/timer.h"
#include "../CORE/video.h"
#include "../CORE/colors.h"
#include "../CORE/high_scores.h"
#include "../GAME/end_screen.h"
#include "../MINI/PONG/pong.h"
#include "../MINI/INVADERS/invaders.h"
#include "../MINI/BREAKOUT/breakout.h"
#include "../MINI/FROG/frog.h"
#include "../MINI/TRON/tron.h"
#include "../MINI/TAPP/tapp.h"
#include "../MINI/PANG/pang.h"
#include "../MINI/GORI/gori.h"
#include "../MINI/FLAPPY/flappy.h"
#include "../main.h"

#include <stdio.h>
#include <stdint.h>

#define YEAR_1972 1972
#define YEAR_1978 1978
#define YEAR_1979 1979
#define YEAR_1981 1981
#define YEAR_1982 1982
#define YEAR_1983 1983
#define YEAR_1989 1989
#define YEAR_1991 1991
#define YEAR_2013 2013
#define STEP_US 16667UL
#define MAX_UPDATES_PER_FRAME 5

typedef void (*GameStorePreviousStateFn)(void);
typedef void (*GameUpdateFn)(void);
typedef void (*GameDrawInterpolatedFn)(float alpha);
typedef int (*GameIsFinishedFn)(void);
typedef enum {
    LOOP_RESULT_FINISHED = 0,
    LOOP_RESULT_ABORTED = 1,
    LOOP_RESULT_FORCED_WIN = 2
} GameLoopResult;

static int g_paused = 0;

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

static void show_option_screen(const char *title)
{
    v_clear(0);
    v_load_palette("palette.dat");
    draw_center_text(title, 80, 15);
    draw_center_text(text_get(TEXT_PRESS_KEY), 100, 15);
    v_present();

    while (!in_keyhit()) {
    }
    while (in_keyhit()) {
        in_poll();
    }
}

static int show_continue_screen(void)
{
    int selected = 0;

    while (in_keyhit()) {
        in_poll();
    }

    while (1) {
        unsigned char yes_color = (selected == 0) ? COL_TEXT_SELECTED : COL_TEXT_NORMAL;
        unsigned char no_color = (selected == 1) ? COL_TEXT_SELECTED : COL_TEXT_NORMAL;

        v_clear(0);
        draw_center_text("CONTINUAR", 70, 15);
        v_puts(120, 110, "SI", yes_color);
        v_puts(180, 110, "NO", no_color);
        v_present();

        while (1) {
            int key = in_poll();
            if (key == IN_KEY_NONE) {
                continue;
            }
            if (key == IN_KEY_LEFT || key == IN_KEY_UP) {
                selected = 0;
                break;
            }
            if (key == IN_KEY_RIGHT || key == IN_KEY_DOWN) {
                selected = 1;
                break;
            }
            if (key == IN_KEY_ENTER) {
                return (selected == 0);
            }
            if (key == IN_KEY_ESC) {
                return 0;
            }
        }
    }
}

static void show_next_game_placeholder(void)
{
    show_option_screen("PROXIMO JUEGO");
}


static GameLoopResult run_fixed_step_loop(GameIsFinishedFn is_finished,
                                          GameStorePreviousStateFn store_previous_state, GameUpdateFn update,
                                          GameDrawInterpolatedFn draw_interpolated)
{
    uint32_t last_us = timer_now_us();
    uint32_t acc = 0;
    int pause_was_down = 0;

    g_paused = 0;

    while (!is_finished()) {
        int pause_down = 0;
        const GameOptions *options = options_get();
        uint32_t now = 0;
        uint32_t frame_us = 0;
        int updates = 0;

        if (Input_Pressed(KEY_P)) {
            g_paused = !g_paused;
        }
        if (kb_down(SC_ESC)) {
            pause_down = 1;
        } else if (options && options->input_mode == INPUT_JOYSTICK) {
            JoystickState state;
            if (in_joystick_state(&state)) {
                if (state.buttons & JOY_BUTTON_ESC) {
                    pause_down = 1;
                }
            }
        }

        if (pause_down && !pause_was_down) {
            int wants_continue = show_continue_screen();
            if (!wants_continue) {
                return LOOP_RESULT_ABORTED;
            }
            in_clear();
            last_us = timer_now_us();
            acc = 0;
        }

        pause_was_down = pause_down;

        now = timer_now_us();

        if (g_paused) {
            last_us = now;
            draw_interpolated((float)acc / (float)STEP_US);
            v_present();
            continue;
        }

        frame_us = now - last_us;

        // OJO: clamp por pausa de DOSBox o tirón
        if (frame_us > 250000UL) frame_us = 250000UL;

        last_us = now;
        acc += frame_us;

        while (acc >= STEP_US && updates < MAX_UPDATES_PER_FRAME) {
            store_previous_state();
            update();
            acc -= STEP_US;
            updates++;
        }

        if (updates >= MAX_UPDATES_PER_FRAME) {
            acc = 0;
        }

        draw_interpolated((float)acc / (float)STEP_US);

        {
            uint32_t frame_elapsed = timer_now_us() - last_us;
            if (frame_elapsed < STEP_US) {
                t_wait_us(STEP_US - frame_elapsed);
            }
        }
    }

    return LOOP_RESULT_FINISHED;
}

static GameLoopResult run_fixed_step_loop_story(GameIsFinishedFn is_finished,
                                                GameStorePreviousStateFn store_previous_state, GameUpdateFn update,
                                                GameDrawInterpolatedFn draw_interpolated)
{
    uint32_t last_us = timer_now_us();
    uint32_t acc = 0;
    int pause_was_down = 0;

    g_paused = 0;

    while (!is_finished()) {
        int pause_down = 0;
        const GameOptions *options = options_get();
        uint32_t now = 0;
        uint32_t frame_us = 0;
        int updates = 0;

#if SHOW_DEBUG
        if (kb_down(SC_LCTRL) && kb_down(SC_W)) {
            return LOOP_RESULT_FORCED_WIN;
        }
#endif

        if (Input_Pressed(KEY_P)) {
            g_paused = !g_paused;
        }
        if (kb_down(SC_ESC)) {
            pause_down = 1;
        } else if (options && options->input_mode == INPUT_JOYSTICK) {
            JoystickState state;
            if (in_joystick_state(&state)) {
                if (state.buttons & JOY_BUTTON_ESC) {
                    pause_down = 1;
                }
            }
        }

        if (pause_down && !pause_was_down) {
            int wants_continue = show_continue_screen();
            if (!wants_continue) {
                return LOOP_RESULT_ABORTED;
            }
            in_clear();
            last_us = timer_now_us();
            acc = 0;
        }

        pause_was_down = pause_down;

        now = timer_now_us();

        if (g_paused) {
            last_us = now;
            draw_interpolated((float)acc / (float)STEP_US);
            v_present();
            continue;
        }

        frame_us = now - last_us;

        // OJO: clamp por pausa de DOSBox o tirón
        if (frame_us > 250000UL) frame_us = 250000UL;

        last_us = now;
        acc += frame_us;

        while (acc >= STEP_US && updates < MAX_UPDATES_PER_FRAME) {
            store_previous_state();
            update();
            acc -= STEP_US;
            updates++;
        }

        if (updates >= MAX_UPDATES_PER_FRAME) {
            acc = 0;
        }

        draw_interpolated((float)acc / (float)STEP_US);

        {
            uint32_t frame_elapsed = timer_now_us() - last_us;
            if (frame_elapsed < STEP_US) {
                t_wait_us(STEP_US - frame_elapsed);
            }
        }
    }

    return LOOP_RESULT_FINISHED;
}

static int handle_minigame_result(int did_win, LaunchMode mode, const char *detail, int sound_enabled,
                                  HighScoreGame game, int year, unsigned char difficulty, uint64_t score)
{
    if (did_win) {
        Game_ShowEndScreen(GAME_END_WIN, detail, sound_enabled, game, year, difficulty, score);
        if (mode == LAUNCH_MODE_HISTORIA) {
            show_next_game_placeholder();
            return 0;
        }
        return show_continue_screen();
    }

    Game_PlayLoseMelody(sound_enabled);
    return 1;
}

static void run_pong_1972(LaunchMode mode)
{
    const GameOptions *options = options_get();
    GameSettings settings;
    int play_again = 1;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (play_again) {
        GameLoopResult loop_result;

        Pong_Init(&settings);

        loop_result = run_fixed_step_loop(Pong_IsFinished, Pong_StorePreviousState,
                                          Pong_Update, Pong_DrawInterpolated);

        Pong_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            play_again = 0;
            continue;
        }
        play_again = handle_minigame_result(Pong_DidWin(), mode, Pong_GetEndDetail(), settings.sound_enabled,
                                            HIGH_SCORE_GAME_PONG, YEAR_1972, settings.difficulty,
                                            Pong_DidWin() ? Pong_GetScore() : 0);
    }
}

static void run_invaders_1978(LaunchMode mode)
{
    const GameOptions *options = options_get();
    GameSettings settings;
    int play_again = 1;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (play_again) {
        GameLoopResult loop_result;

        Invaders_Init(&settings);

        loop_result = run_fixed_step_loop(Invaders_IsFinished, Invaders_StorePreviousState,
                                          Invaders_Update, Invaders_DrawInterpolated);

        Invaders_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            play_again = 0;
            continue;
        }
        play_again = handle_minigame_result(Invaders_DidWin(), mode, Invaders_GetEndDetail(),
                                            settings.sound_enabled, HIGH_SCORE_GAME_INVADERS, YEAR_1978,
                                            settings.difficulty,
                                            Invaders_DidWin() ? Invaders_GetScore() : 0);
    }
}

static void run_breakout_1979(LaunchMode mode)
{
    const GameOptions *options = options_get();
    GameSettings settings;
    int play_again = 1;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (play_again) {
        GameLoopResult loop_result;

        Breakout_Init(&settings);

        loop_result = run_fixed_step_loop(Breakout_IsFinished, Breakout_StorePreviousState,
                                          Breakout_Update, Breakout_DrawInterpolated);

        Breakout_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            play_again = 0;
            continue;
        }
        play_again = handle_minigame_result(Breakout_DidWin(), mode, Breakout_GetEndDetail(),
                                            settings.sound_enabled, HIGH_SCORE_GAME_BREAKOUT, YEAR_1979,
                                            settings.difficulty,
                                            Breakout_DidWin() ? Breakout_GetScore() : 0);
    }
}

static void run_frog_1981(LaunchMode mode)
{
    const GameOptions *options = options_get();
    GameSettings settings;
    int play_again = 1;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (play_again) {
        GameLoopResult loop_result;

        Frog_Init(&settings);

        loop_result = run_fixed_step_loop(Frog_IsFinished, Frog_StorePreviousState,
                                          Frog_Update, Frog_DrawInterpolated);

        Frog_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            play_again = 0;
            continue;
        }
        play_again = handle_minigame_result(Frog_DidWin(), mode, Frog_GetEndDetail(),
                                            settings.sound_enabled, HIGH_SCORE_GAME_FROG, YEAR_1981,
                                            settings.difficulty,
                                            Frog_DidWin() ? Frog_GetScore() : 0);
    }
}

static void run_tapp_1983(LaunchMode mode)
{
    const GameOptions *options = options_get();
    GameSettings settings;
    int play_again = 1;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (play_again) {
        GameLoopResult loop_result;

        Tapp_Init(&settings);

        loop_result = run_fixed_step_loop(Tapp_IsFinished, Tapp_StorePreviousState,
                                          Tapp_Update, Tapp_DrawInterpolated);

        Tapp_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            play_again = 0;
            continue;
        }
        play_again = handle_minigame_result(Tapp_DidWin(), mode, Tapp_GetEndDetail(),
                                            settings.sound_enabled, HIGH_SCORE_GAME_TAPP, YEAR_1983,
                                            settings.difficulty,
                                            Tapp_DidWin() ? Tapp_GetScore() : 0);
    }
}

static void run_tron_1982(LaunchMode mode)
{
    const GameOptions *options = options_get();
    GameSettings settings;
    int play_again = 1;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (play_again) {
        GameLoopResult loop_result;

        Tron_Init(&settings);

        loop_result = run_fixed_step_loop(Tron_IsFinished, Tron_StorePreviousState,
                                          Tron_Update, Tron_DrawInterpolated);

        Tron_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            play_again = 0;
            continue;
        }
        play_again = handle_minigame_result(Tron_DidWin(), mode, Tron_GetEndDetail(),
                                            settings.sound_enabled, HIGH_SCORE_GAME_TRON, YEAR_1982,
                                            settings.difficulty,
                                            Tron_DidWin() ? Tron_GetScore() : 0);
    }
}

static void run_pang_1989(LaunchMode mode)
{
    const GameOptions *options = options_get();
    GameSettings settings;
    int play_again = 1;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (play_again) {
        GameLoopResult loop_result;

        Pang_Init(&settings);

        loop_result = run_fixed_step_loop(Pang_IsFinished, Pang_StorePreviousState,
                                          Pang_Update, Pang_DrawInterpolated);

        Pang_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            play_again = 0;
            continue;
        }
        play_again = handle_minigame_result(Pang_DidWin(), mode, Pang_GetEndDetail(),
                                            settings.sound_enabled, HIGH_SCORE_GAME_PANG, YEAR_1989,
                                            settings.difficulty,
                                            Pang_DidWin() ? Pang_GetScore() : 0);
    }
}

static void run_gori_1991(LaunchMode mode)
{
    const GameOptions *options = options_get();
    GameSettings settings;
    int play_again = 1;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (play_again) {
        GameLoopResult loop_result;

        Gori_Init(&settings);

        loop_result = run_fixed_step_loop(Gori_IsFinished, Gori_StorePreviousState,
                                          Gori_Update, Gori_DrawInterpolated);

        Gori_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            play_again = 0;
            continue;
        }
        play_again = handle_minigame_result(Gori_DidWin(), mode, Gori_GetEndDetail(),
                                            settings.sound_enabled, HIGH_SCORE_GAME_GORI, YEAR_1991,
                                            settings.difficulty,
                                            Gori_DidWin() ? Gori_GetScore() : 0);
    }
}

static void run_flappy_2013(LaunchMode mode)
{
    const GameOptions *options = options_get();
    GameSettings settings;
    int play_again = 1;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (play_again) {
        GameLoopResult loop_result;

        Flappy_Init(&settings);

        loop_result = run_fixed_step_loop(Flappy_IsFinished, Flappy_StorePreviousState,
                                          Flappy_Update, Flappy_DrawInterpolated);

        Flappy_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            play_again = 0;
            continue;
        }
        play_again = handle_minigame_result(Flappy_DidWin(), mode, Flappy_GetEndDetail(),
                                            settings.sound_enabled, HIGH_SCORE_GAME_FLAPPY, YEAR_2013,
                                            settings.difficulty,
                                            Flappy_DidWin() ? Flappy_GetScore() : 0);
    }
}

static int run_pong_1972_story(uint64_t *out_score, uint32_t *out_retries)
{
    const GameOptions *options = options_get();
    GameSettings settings;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (1) {
        GameLoopResult loop_result;

        Pong_Init(&settings);

        loop_result = run_fixed_step_loop_story(Pong_IsFinished, Pong_StorePreviousState,
                                                Pong_Update, Pong_DrawInterpolated);

        Pong_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            return 0;
        }
        if (loop_result == LOOP_RESULT_FORCED_WIN || Pong_DidWin()) {
            if (out_score) {
                *out_score = Pong_GetScore();
            }
            return 1;
        }
        if (out_retries) {
            (*out_retries)++;
        }
        Game_PlayLoseMelody(settings.sound_enabled);
    }
}

static int run_invaders_1978_story(uint64_t *out_score, uint32_t *out_retries)
{
    const GameOptions *options = options_get();
    GameSettings settings;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (1) {
        GameLoopResult loop_result;

        Invaders_Init(&settings);

        loop_result = run_fixed_step_loop_story(Invaders_IsFinished, Invaders_StorePreviousState,
                                                Invaders_Update, Invaders_DrawInterpolated);

        Invaders_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            return 0;
        }
        if (loop_result == LOOP_RESULT_FORCED_WIN || Invaders_DidWin()) {
            if (out_score) {
                *out_score = Invaders_GetScore();
            }
            return 1;
        }
        if (out_retries) {
            (*out_retries)++;
        }
        Game_PlayLoseMelody(settings.sound_enabled);
    }
}

static int run_breakout_1979_story(uint64_t *out_score, uint32_t *out_retries)
{
    const GameOptions *options = options_get();
    GameSettings settings;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (1) {
        GameLoopResult loop_result;

        Breakout_Init(&settings);

        loop_result = run_fixed_step_loop_story(Breakout_IsFinished, Breakout_StorePreviousState,
                                                Breakout_Update, Breakout_DrawInterpolated);

        Breakout_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            return 0;
        }
        if (loop_result == LOOP_RESULT_FORCED_WIN || Breakout_DidWin()) {
            if (out_score) {
                *out_score = Breakout_GetScore();
            }
            return 1;
        }
        if (out_retries) {
            (*out_retries)++;
        }
        Game_PlayLoseMelody(settings.sound_enabled);
    }
}

static int run_frog_1981_story(uint64_t *out_score, uint32_t *out_retries)
{
    const GameOptions *options = options_get();
    GameSettings settings;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (1) {
        GameLoopResult loop_result;

        Frog_Init(&settings);

        loop_result = run_fixed_step_loop_story(Frog_IsFinished, Frog_StorePreviousState,
                                                Frog_Update, Frog_DrawInterpolated);

        Frog_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            return 0;
        }
        if (loop_result == LOOP_RESULT_FORCED_WIN || Frog_DidWin()) {
            if (out_score) {
                *out_score = Frog_GetScore();
            }
            return 1;
        }
        if (out_retries) {
            (*out_retries)++;
        }
        Game_PlayLoseMelody(settings.sound_enabled);
    }
}

static int run_tron_1982_story(uint64_t *out_score, uint32_t *out_retries)
{
    const GameOptions *options = options_get();
    GameSettings settings;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (1) {
        GameLoopResult loop_result;

        Tron_Init(&settings);

        loop_result = run_fixed_step_loop_story(Tron_IsFinished, Tron_StorePreviousState,
                                                Tron_Update, Tron_DrawInterpolated);

        Tron_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            return 0;
        }
        if (loop_result == LOOP_RESULT_FORCED_WIN || Tron_DidWin()) {
            if (out_score) {
                *out_score = Tron_GetScore();
            }
            return 1;
        }
        if (out_retries) {
            (*out_retries)++;
        }
        Game_PlayLoseMelody(settings.sound_enabled);
    }
}

static int run_tapp_1983_story(uint64_t *out_score, uint32_t *out_retries)
{
    const GameOptions *options = options_get();
    GameSettings settings;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (1) {
        GameLoopResult loop_result;

        Tapp_Init(&settings);

        loop_result = run_fixed_step_loop_story(Tapp_IsFinished, Tapp_StorePreviousState,
                                                Tapp_Update, Tapp_DrawInterpolated);

        Tapp_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            return 0;
        }
        if (loop_result == LOOP_RESULT_FORCED_WIN || Tapp_DidWin()) {
            if (out_score) {
                *out_score = Tapp_GetScore();
            }
            return 1;
        }
        if (out_retries) {
            (*out_retries)++;
        }
        Game_PlayLoseMelody(settings.sound_enabled);
    }
}

static int run_pang_1989_story(uint64_t *out_score, uint32_t *out_retries)
{
    const GameOptions *options = options_get();
    GameSettings settings;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (1) {
        GameLoopResult loop_result;

        Pang_Init(&settings);

        loop_result = run_fixed_step_loop_story(Pang_IsFinished, Pang_StorePreviousState,
                                                Pang_Update, Pang_DrawInterpolated);

        Pang_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            return 0;
        }
        if (loop_result == LOOP_RESULT_FORCED_WIN || Pang_DidWin()) {
            if (out_score) {
                *out_score = Pang_GetScore();
            }
            return 1;
        }
        if (out_retries) {
            (*out_retries)++;
        }
        Game_PlayLoseMelody(settings.sound_enabled);
    }
}

static int run_gori_1991_story(uint64_t *out_score, uint32_t *out_retries)
{
    const GameOptions *options = options_get();
    GameSettings settings;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (1) {
        GameLoopResult loop_result;

        Gori_Init(&settings);

        loop_result = run_fixed_step_loop_story(Gori_IsFinished, Gori_StorePreviousState,
                                                Gori_Update, Gori_DrawInterpolated);

        Gori_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            return 0;
        }
        if (loop_result == LOOP_RESULT_FORCED_WIN || Gori_DidWin()) {
            if (out_score) {
                *out_score = Gori_GetScore();
            }
            return 1;
        }
        if (out_retries) {
            (*out_retries)++;
        }
        Game_PlayLoseMelody(settings.sound_enabled);
    }
}

static int run_flappy_2013_story(uint64_t *out_score, uint32_t *out_retries)
{
    const GameOptions *options = options_get();
    GameSettings settings;

    settings.difficulty = options->difficulty;
    settings.sound_enabled = options->sound_enabled;
    settings.input_mode = options->input_mode;
    settings.game_speed = options->game_speed;
    settings.speed_multiplier = options_speed_multiplier();

    while (1) {
        GameLoopResult loop_result;

        Flappy_Init(&settings);

        loop_result = run_fixed_step_loop_story(Flappy_IsFinished, Flappy_StorePreviousState,
                                                Flappy_Update, Flappy_DrawInterpolated);

        Flappy_End();
        if (loop_result == LOOP_RESULT_ABORTED) {
            return 0;
        }
        if (loop_result == LOOP_RESULT_FORCED_WIN || Flappy_DidWin()) {
            if (out_score) {
                *out_score = Flappy_GetScore();
            }
            return 1;
        }
        if (out_retries) {
            (*out_retries)++;
        }
        Game_PlayLoseMelody(settings.sound_enabled);
    }
}

void launch_year_game(int year, LaunchMode mode)
{
    char year_text[8];

    if (year == YEAR_1972) {
        run_pong_1972(mode);
        return;
    }

    if (year == YEAR_1978) {
        run_invaders_1978(mode);
        return;
    }

    if (year == YEAR_1979) {
        run_breakout_1979(mode);
        return;
    }

    if (year == YEAR_1981) {
        run_frog_1981(mode);
        return;
    }

    if (year == YEAR_1982) {
        run_tron_1982(mode);
        return;
    }

    if (year == YEAR_1983) {
        run_tapp_1983(mode);
        return;
    }

    if (year == YEAR_1989) {
        run_pang_1989(mode);
        return;
    }

    if (year == YEAR_1991) {
        run_gori_1991(mode);
        return;
    }

    if (year == YEAR_2013) {
        run_flappy_2013(mode);
        return;
    }

    snprintf(year_text, sizeof(year_text), "%d", year);
    show_option_screen(year_text);
}

int launch_year_game_story(int year, uint64_t *out_score, uint32_t *out_retries)
{
    if (out_score) {
        *out_score = 0;
    }
    if (out_retries) {
        *out_retries = 0;
    }

    if (year == YEAR_1972) {
        return run_pong_1972_story(out_score, out_retries);
    }

    if (year == YEAR_1978) {
        return run_invaders_1978_story(out_score, out_retries);
    }

    if (year == YEAR_1979) {
        return run_breakout_1979_story(out_score, out_retries);
    }

    if (year == YEAR_1981) {
        return run_frog_1981_story(out_score, out_retries);
    }

    if (year == YEAR_1982) {
        return run_tron_1982_story(out_score, out_retries);
    }

    if (year == YEAR_1983) {
        return run_tapp_1983_story(out_score, out_retries);
    }

    if (year == YEAR_1989) {
        return run_pang_1989_story(out_score, out_retries);
    }

    if (year == YEAR_1991) {
        return run_gori_1991_story(out_score, out_retries);
    }

    if (year == YEAR_2013) {
        return run_flappy_2013_story(out_score, out_retries);
    }

    return 0;
}
