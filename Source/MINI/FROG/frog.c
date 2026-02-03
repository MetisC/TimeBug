#include "frog.h"

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
#include <math.h>
#include <stdint.h>
#include <malloc.h>

#define FROG_TILE 16
#define FROG_GRID_COLS 20
#define FROG_GRID_ROWS 11
#define FROG_GRID_Y 8
#define FROG_MAX_SPRITE_PIXELS 4096
#define FROG_TICKS_PER_SECOND 60
#define FROG_START_LIVES 3
#define FROG_HOP_ANIM_TICKS 6
#define FROG_RESPAWN_DELAY_TICKS 20

#define FROG_SCORE_HOP 10
#define FROG_SCORE_GOAL 1000
#define FROG_SCORE_TIME_BONUS 500

#define FROG_ROW_GOAL 0
#define FROG_ROW_RIVER_START 1
#define FROG_ROW_RIVER_COUNT 4
#define FROG_ROW_MEDIAN 5
#define FROG_ROW_ROAD_START 6
#define FROG_ROW_ROAD_COUNT 4
#define FROG_ROW_START_TOP 10
#define FROG_ROW_START_BOTTOM 10

#define FROG_COLOR_START 2
#define FROG_COLOR_ROAD 8
#define FROG_COLOR_MEDIAN 2
#define FROG_COLOR_RIVER 1
#define FROG_COLOR_GOAL 3

#define FROG_COLOR_WATER_BLUE_MED 165
#define FROG_COLOR_WATER_BLUE_DARK 160
#define FROG_COLOR_ROAD_GRAY_MED 5
#define FROG_COLOR_ROAD_GRAY_DARK 3
#define FROG_COLOR_LANE_WHITE 10
#define FROG_COLOR_SAFE_ZONE 33
#define FROG_COLOR_SEPARATOR 1

#define FROG_WATER_PERIOD 24
#define FROG_WATER_SCROLL_FRAMES 3

#define FROG_MAX_ROAD_LANES 4
#define FROG_MAX_RIVER_LANES 4
#define FROG_MAX_VEHICLES_PER_LANE 4
#define FROG_MAX_PLATFORMS_PER_LANE 4
#define FROG_GOAL_SLOTS 5

typedef struct {
    float road_speed[FROG_MAX_ROAD_LANES];
    float river_speed[FROG_MAX_RIVER_LANES];
    int road_count[FROG_MAX_ROAD_LANES];
    int river_count[FROG_MAX_RIVER_LANES];
    int timer_seconds;
} FrogParams;

typedef struct {
    unsigned short w;
    unsigned short h;
    unsigned char far *pixels;
} FrogSprite;

typedef struct {
    float x[FROG_MAX_VEHICLES_PER_LANE];
    float x_prev[FROG_MAX_VEHICLES_PER_LANE];
} FrogLanePositions;

typedef struct {
    float x[FROG_MAX_PLATFORMS_PER_LANE];
    float x_prev[FROG_MAX_PLATFORMS_PER_LANE];
} FrogPlatformPositions;

typedef enum {
    FROG_DIR_UP = 0,
    FROG_DIR_RIGHT,
    FROG_DIR_DOWN,
    FROG_DIR_LEFT
} FrogDirection;

typedef enum {
    FROG_PLATFORM_LOG = 0,
    FROG_PLATFORM_TURTLE
} FrogPlatformType;

static const int g_road_rows[FROG_MAX_ROAD_LANES] = {6, 7, 8, 9};
static const int g_river_rows[FROG_MAX_RIVER_LANES] = {1, 2, 3, 4};
static const int g_lane_direction[FROG_MAX_ROAD_LANES] = {1, -1, 1, -1};
static const int g_river_direction[FROG_MAX_RIVER_LANES] = {-1, 1, -1, 1};
static const int g_road_vehicle_type[FROG_MAX_ROAD_LANES] = {0, 1, 2, 0};
static const FrogPlatformType g_river_platform_type[FROG_MAX_RIVER_LANES] = {
    FROG_PLATFORM_LOG,
    FROG_PLATFORM_TURTLE,
    FROG_PLATFORM_LOG,
    FROG_PLATFORM_TURTLE
};
static const int g_log_length[FROG_MAX_RIVER_LANES] = {3, 0, 4, 0};
static const int g_goal_slots[FROG_GOAL_SLOTS] = {1, 5, 9, 13, 17};

static GameSettings g_settings;
static FrogParams g_params;
static int g_finished = 0;
static int g_did_win = 0;
static int g_sound_enabled = 0;
static int g_use_keyboard = 1;

static FrogSprite g_frog1;
static FrogSprite g_frog2;
static FrogSprite g_car1;
static FrogSprite g_car2;
static FrogSprite g_truck1;
static FrogSprite g_tree1;
static FrogSprite g_tree2;
static FrogSprite g_tree3;
static FrogSprite g_turtle1;
static FrogSprite g_lilly1;
static int g_sprites_loaded = 0;

static FrogLanePositions g_road_positions[FROG_MAX_ROAD_LANES];
static FrogPlatformPositions g_river_positions[FROG_MAX_RIVER_LANES];

static float g_frog_x = 0.0f;
static float g_frog_y = 0.0f;
static float g_frog_x_prev = 0.0f;
static float g_frog_y_prev = 0.0f;
static FrogDirection g_frog_dir = FROG_DIR_UP;
static int g_hop_ticks = 0;
static int g_move_held = 0;
static int g_lives = 0;
static int g_timer_ticks = 0;
static uint64_t g_score = 0;
static uint64_t g_final_score = 0;
static int g_respawn_delay_ticks = 0;
static char g_end_detail[32] = "";
static char g_last_death_reason[32] = "";
static int g_frame_counter = 0;
static int g_water_scroll_px = 0;
static int g_water_scroll_acc = 0;

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

static void frog_select_params(unsigned char difficulty, FrogParams *params)
{
    if (!params) {
        return;
    }

    switch (difficulty) {
    case DIFFICULTY_EASY:
        params->road_speed[0] = 0.60f;
        params->road_speed[1] = 0.70f;
        params->road_speed[2] = 0.80f;
        params->road_speed[3] = 0.90f;
        params->river_speed[0] = 0.45f;
        params->river_speed[1] = 0.55f;
        params->river_speed[2] = 0.50f;
        params->river_speed[3] = 0.60f;
        params->road_count[0] = 2;
        params->road_count[1] = 2;
        params->road_count[2] = 2;
        params->road_count[3] = 2;
        params->river_count[0] = 2;
        params->river_count[1] = 2;
        params->river_count[2] = 2;
        params->river_count[3] = 2;
        params->timer_seconds = 45;
        break;
    case DIFFICULTY_HARD:
        params->road_speed[0] = 1.00f;
        params->road_speed[1] = 1.20f;
        params->road_speed[2] = 1.30f;
        params->road_speed[3] = 1.40f;
        params->river_speed[0] = 0.85f;
        params->river_speed[1] = 1.00f;
        params->river_speed[2] = 0.90f;
        params->river_speed[3] = 1.10f;
        params->road_count[0] = 3;
        params->road_count[1] = 3;
        params->road_count[2] = 4;
        params->road_count[3] = 3;
        params->river_count[0] = 3;
        params->river_count[1] = 3;
        params->river_count[2] = 3;
        params->river_count[3] = 3;
        params->timer_seconds = 28;
        break;
    case DIFFICULTY_NORMAL:
    default:
        params->road_speed[0] = 0.80f;
        params->road_speed[1] = 0.95f;
        params->road_speed[2] = 1.05f;
        params->road_speed[3] = 1.15f;
        params->river_speed[0] = 0.65f;
        params->river_speed[1] = 0.75f;
        params->river_speed[2] = 0.70f;
        params->river_speed[3] = 0.80f;
        params->road_count[0] = 3;
        params->road_count[1] = 3;
        params->road_count[2] = 3;
        params->road_count[3] = 3;
        params->river_count[0] = 3;
        params->river_count[1] = 3;
        params->river_count[2] = 3;
        params->river_count[3] = 3;
        params->timer_seconds = 35;
        break;
    }
}

static int frog_alloc_sprite_pixels(FrogSprite *sprite)
{
    if (!sprite) {
        return 0;
    }
    if (!sprite->pixels) {
        sprite->pixels = (unsigned char far *)_fmalloc(FROG_MAX_SPRITE_PIXELS);
        if (!sprite->pixels) {
            return 0;
        }
    }
    return 1;
}

static int frog_load_sprite(const char *path, FrogSprite *sprite)
{
    if (!sprite || !path) {
        return 0;
    }

    sprite->w = 0;
    sprite->h = 0;

    if (!frog_alloc_sprite_pixels(sprite)) {
        return 0;
    }

    return sprite_dat_load_auto(path, &sprite->w, &sprite->h, sprite->pixels,
                                (unsigned long)FROG_MAX_SPRITE_PIXELS);
}

static void frog_load_sprites(void)
{
    if (g_sprites_loaded) {
        return;
    }

    frog_load_sprite("SPRITES\\frog1.dat", &g_frog1);
    frog_load_sprite("SPRITES\\frog2.dat", &g_frog2);
    frog_load_sprite("SPRITES\\car1.dat", &g_car1);
    frog_load_sprite("SPRITES\\car2.dat", &g_car2);
    frog_load_sprite("SPRITES\\truck1.dat", &g_truck1);
    frog_load_sprite("SPRITES\\tree1.dat", &g_tree1);
    frog_load_sprite("SPRITES\\tree2.dat", &g_tree2);
    frog_load_sprite("SPRITES\\tree3.dat", &g_tree3);
    frog_load_sprite("SPRITES\\turtle1.dat", &g_turtle1);
    frog_load_sprite("SPRITES\\lilly1.dat", &g_lilly1);

    g_sprites_loaded = 1;
}

static void frog_free_sprite(FrogSprite *sprite)
{
    if (!sprite || !sprite->pixels) {
        return;
    }

    _ffree(sprite->pixels);
    sprite->pixels = NULL;
    sprite->w = 0;
    sprite->h = 0;
}

static void frog_free_sprites(void)
{
    if (!g_sprites_loaded) {
        return;
    }

    frog_free_sprite(&g_frog1);
    frog_free_sprite(&g_frog2);
    frog_free_sprite(&g_car1);
    frog_free_sprite(&g_car2);
    frog_free_sprite(&g_truck1);
    frog_free_sprite(&g_tree1);
    frog_free_sprite(&g_tree2);
    frog_free_sprite(&g_tree3);
    frog_free_sprite(&g_turtle1);
    frog_free_sprite(&g_lilly1);

    g_sprites_loaded = 0;
}

static int frog_sprite_width(const FrogSprite *sprite, int fallback)
{
    if (!sprite || sprite->w == 0) {
        return fallback;
    }
    return sprite->w;
}

static int frog_sprite_height(const FrogSprite *sprite, int fallback)
{
    if (!sprite || sprite->h == 0) {
        return fallback;
    }
    return sprite->h;
}

static void frog_blit_sprite(int x, int y, const FrogSprite *sprite)
{
    if (!sprite || sprite->w == 0 || sprite->h == 0) {
        return;
    }

    v_blit_sprite(x, y, sprite->w, sprite->h, (const unsigned char far *)sprite->pixels, 0);
}

static void frog_blit_sprite_flipped(int x, int y, const FrogSprite *sprite, int flip_x, int flip_y)
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
        int src_y = flip_y ? (h - 1 - sy) : sy;
        for (sx = 0; sx < w; ++sx) {
            int src_x = flip_x ? (w - 1 - sx) : sx;
            unsigned char color = sprite->pixels[src_y * w + src_x];
            if (color != 0) {
                v_putpixel(x + sx, y + sy, color);
            }
        }
    }
}

static int frog_round_to_int(float value)
{
    return (int)(value + (value >= 0.0f ? 0.5f : -0.5f));
}

static void frog_blit_sprite_rotated_45(int x, int y, const FrogSprite *sprite, int clockwise)
{
    int sx;
    int sy;
    int w;
    int h;
    int cx;
    int cy;

    if (!sprite || sprite->w == 0 || sprite->h == 0) {
        return;
    }

    w = sprite->w;
    h = sprite->h;

    cx = (w - 1) / 2;
    cy = (h - 1) / 2;

    for (sy = 0; sy < h; ++sy) {
        for (sx = 0; sx < w; ++sx) {
            unsigned char color = sprite->pixels[sy * w + sx];
            if (color != 0) {
                int dx = sx - cx;
                int dy = sy - cy;

                int rx;
                int ry;

                if (clockwise) {
                    rx = -dy;
                    ry =  dx;
                } else {
                    rx =  dy;
                    ry = -dx;
                }

                v_putpixel(
                    x + rx + cx,
                    y + ry + cy,
                    color
                );
            }
        }
    }
}

static void frog_reset_frog_position(void)
{
    int frog_w = frog_sprite_width(&g_frog1, FROG_TILE);

    g_frog_x = (float)((VIDEO_WIDTH - frog_w) / 2);
    g_frog_y = (float)(FROG_GRID_Y + (FROG_ROW_START_BOTTOM * FROG_TILE));
    g_frog_x_prev = g_frog_x;
    g_frog_y_prev = g_frog_y;
    g_frog_dir = FROG_DIR_UP;
    g_hop_ticks = 0;
}

static void frog_spawn_lanes(void)
{
    int lane;

    for (lane = 0; lane < FROG_MAX_ROAD_LANES; ++lane) {
        int count = g_params.road_count[lane];
        int sprite_w = FROG_TILE;
        float spacing = 0.0f;
        int i;

        if (g_road_vehicle_type[lane] == 2) {
            sprite_w = frog_sprite_width(&g_truck1, FROG_TILE * 2);
        } else if (g_road_vehicle_type[lane] == 1) {
            sprite_w = frog_sprite_width(&g_car2, FROG_TILE * 2);
        } else {
            sprite_w = frog_sprite_width(&g_car1, FROG_TILE * 2);
        }

        if (count > 0) {
            spacing = (float)(VIDEO_WIDTH + sprite_w) / (float)count;
        }

        for (i = 0; i < count; ++i) {
            g_road_positions[lane].x[i] = (float)(-sprite_w) + spacing * (float)i;
            g_road_positions[lane].x_prev[i] = g_road_positions[lane].x[i];
        }
    }

    for (lane = 0; lane < FROG_MAX_RIVER_LANES; ++lane) {
        int count = g_params.river_count[lane];
        int sprite_w = FROG_TILE;
        float spacing = 0.0f;
        int i;

        if (g_river_platform_type[lane] == FROG_PLATFORM_LOG) {
            sprite_w = g_log_length[lane] * FROG_TILE;
        } else {
            sprite_w = frog_sprite_width(&g_turtle1, FROG_TILE);
        }

        if (count > 0) {
            spacing = (float)(VIDEO_WIDTH + sprite_w) / (float)count;
        }

        for (i = 0; i < count; ++i) {
            g_river_positions[lane].x[i] = (float)(-sprite_w) + spacing * (float)i;
            g_river_positions[lane].x_prev[i] = g_river_positions[lane].x[i];
        }
    }
}

static int frog_rect_intersect(float x0, float y0, float w0, float h0, float x1, float y1, float w1, float h1)
{
    if (x0 + w0 <= x1) {
        return 0;
    }
    if (x1 + w1 <= x0) {
        return 0;
    }
    if (y0 + h0 <= y1) {
        return 0;
    }
    if (y1 + h1 <= y0) {
        return 0;
    }
    return 1;
}

static int frog_lane_for_row(int row, const int *rows, int row_count)
{
    int i;

    for (i = 0; i < row_count; ++i) {
        if (rows[i] == row) {
            return i;
        }
    }

    return -1;
}

static void frog_reset_timer(void)
{
    g_timer_ticks = g_params.timer_seconds * FROG_TICKS_PER_SECOND;
}

static void frog_kill(const char *reason)
{
    if (g_sound_enabled) {
        sound_play_tone(260, 80);
    }

    g_lives--;
    if (reason) {
        snprintf(g_last_death_reason, sizeof(g_last_death_reason), "%s", reason);
    }

    if (g_lives <= 0) {
        g_finished = 1;
        g_did_win = 0;
        return;
    }

    frog_reset_timer();
    frog_reset_frog_position();
    g_respawn_delay_ticks = FROG_RESPAWN_DELAY_TICKS;
}

void Frog_Init(const GameSettings *settings)
{
    g_settings = *settings;
    frog_select_params(g_settings.difficulty, &g_params);
    frog_load_sprites();

    g_finished = 0;
    g_did_win = 0;
    g_sound_enabled = g_settings.sound_enabled ? 1 : 0;

    g_use_keyboard = 1;
    if (g_settings.input_mode == INPUT_JOYSTICK) {
        if (in_joystick_available()) {
            g_use_keyboard = 0;
        }
    }

    g_lives = FROG_START_LIVES;
    g_score = 0;
    g_final_score = 0;
    g_last_death_reason[0] = '\0';
    g_frame_counter = 0;
    g_water_scroll_px = 0;
    g_water_scroll_acc = 0;

    frog_reset_frog_position();
    frog_reset_timer();
    frog_spawn_lanes();
    g_respawn_delay_ticks = 0;
    g_move_held = 0;

    Frog_StorePreviousState();

    while (in_keyhit()) {
        in_poll();
    }
}

void Frog_StorePreviousState(void)
{
    int lane;
    int i;

    g_frog_x_prev = g_frog_x;
    g_frog_y_prev = g_frog_y;

    for (lane = 0; lane < FROG_MAX_ROAD_LANES; ++lane) {
        for (i = 0; i < g_params.road_count[lane]; ++i) {
            g_road_positions[lane].x_prev[i] = g_road_positions[lane].x[i];
        }
    }

    for (lane = 0; lane < FROG_MAX_RIVER_LANES; ++lane) {
        for (i = 0; i < g_params.river_count[lane]; ++i) {
            g_river_positions[lane].x_prev[i] = g_river_positions[lane].x[i];
        }
    }
}

static void frog_handle_input(void)
{
    int move_x = 0;
    int move_y = 0;

    if (g_use_keyboard) {
        if (kb_down(SC_UP) || kb_down(SC_W)) {
            move_y = -1;
        } else if (kb_down(SC_DOWN) || kb_down(SC_S)) {
            move_y = 1;
        } else if (kb_down(SC_LEFT) || kb_down(SC_A)) {
            move_x = -1;
        } else if (kb_down(SC_RIGHT) || kb_down(SC_D)) {
            move_x = 1;
        }
    } else {
        int dx = 0;
        int dy = 0;
        unsigned char buttons = 0;

        if (in_joystick_direction(&dx, &dy, &buttons)) {
            if (dy < 0) {
                move_y = -1;
            } else if (dy > 0) {
                move_y = 1;
            } else if (dx < 0) {
                move_x = -1;
            } else if (dx > 0) {
                move_x = 1;
            }
        }
    }

    if (move_x == 0 && move_y == 0) {
        g_move_held = 0;
        return;
    }

    if (g_move_held) {
        return;
    }

    g_move_held = 1;

    if (move_y < 0) {
        g_frog_dir = FROG_DIR_UP;
    } else if (move_y > 0) {
        g_frog_dir = FROG_DIR_DOWN;
    } else if (move_x < 0) {
        g_frog_dir = FROG_DIR_LEFT;
    } else if (move_x > 0) {
        g_frog_dir = FROG_DIR_RIGHT;
    }

    if (move_x != 0 || move_y != 0) {
        float new_x = g_frog_x + (float)(move_x * FROG_TILE);
        float new_y = g_frog_y + (float)(move_y * FROG_TILE);
        int frog_w = frog_sprite_width(&g_frog1, FROG_TILE);

        new_x = clampf(new_x, 0.0f, (float)(VIDEO_WIDTH - frog_w));
        new_y = clampf(new_y, (float)FROG_GRID_Y, (float)(FROG_GRID_Y + (FROG_GRID_ROWS - 1) * FROG_TILE));

        if (new_y < g_frog_y) {
            g_score += FROG_SCORE_HOP;
        }

        g_frog_x = new_x;
        g_frog_y = new_y;
        g_hop_ticks = FROG_HOP_ANIM_TICKS;

        if (g_sound_enabled) {
            sound_play_tone(520, 20);
        }
    }
}

static void frog_update_lanes(void)
{
    int lane;
    int i;

    for (lane = 0; lane < FROG_MAX_ROAD_LANES; ++lane) {
        int count = g_params.road_count[lane];
        int dir = g_lane_direction[lane];
        float speed = g_params.road_speed[lane] * g_settings.speed_multiplier;
        int sprite_w = FROG_TILE;

        if (g_road_vehicle_type[lane] == 2) {
            sprite_w = frog_sprite_width(&g_truck1, FROG_TILE * 2);
        } else if (g_road_vehicle_type[lane] == 1) {
            sprite_w = frog_sprite_width(&g_car2, FROG_TILE * 2);
        } else {
            sprite_w = frog_sprite_width(&g_car1, FROG_TILE * 2);
        }

        for (i = 0; i < count; ++i) {
            g_road_positions[lane].x[i] += speed * dir;
            if (dir > 0 && g_road_positions[lane].x[i] > VIDEO_WIDTH) {
                g_road_positions[lane].x[i] = -sprite_w;
                g_road_positions[lane].x_prev[i] = g_road_positions[lane].x[i];
            } else if (dir < 0 && g_road_positions[lane].x[i] < -sprite_w) {
                g_road_positions[lane].x[i] = (float)VIDEO_WIDTH;
                g_road_positions[lane].x_prev[i] = g_road_positions[lane].x[i];
            }
        }
    }

    for (lane = 0; lane < FROG_MAX_RIVER_LANES; ++lane) {
        int count = g_params.river_count[lane];
        int dir = g_river_direction[lane];
        float speed = g_params.river_speed[lane] * g_settings.speed_multiplier;
        int sprite_w = FROG_TILE;

        if (g_river_platform_type[lane] == FROG_PLATFORM_LOG) {
            sprite_w = g_log_length[lane] * FROG_TILE;
        } else {
            sprite_w = frog_sprite_width(&g_turtle1, FROG_TILE);
        }

        for (i = 0; i < count; ++i) {
            g_river_positions[lane].x[i] += speed * dir;
            if (dir > 0 && g_river_positions[lane].x[i] > VIDEO_WIDTH) {
                g_river_positions[lane].x[i] = -sprite_w;
                g_river_positions[lane].x_prev[i] = g_river_positions[lane].x[i];
            } else if (dir < 0 && g_river_positions[lane].x[i] < -sprite_w) {
                g_river_positions[lane].x[i] = (float)VIDEO_WIDTH;
                g_river_positions[lane].x_prev[i] = g_river_positions[lane].x[i];
            }
        }
    }
}

static void frog_handle_collisions(void)
{
    int frog_w = frog_sprite_width(&g_frog1, FROG_TILE);
    int frog_h = frog_sprite_height(&g_frog1, FROG_TILE);
    int row = (int)((g_frog_y - FROG_GRID_Y) / FROG_TILE);

    if (row == FROG_ROW_GOAL) {
        int col = (int)((g_frog_x + (frog_w / 2)) / FROG_TILE);
        int i;
        int on_slot = 0;

        for (i = 0; i < FROG_GOAL_SLOTS; ++i) {
            if (col == g_goal_slots[i]) {
                on_slot = 1;
                break;
            }
        }

        if (on_slot) {
            g_score += FROG_SCORE_GOAL;
            g_did_win = 1;
            g_finished = 1;
        } else {
            frog_kill("META FALLIDA");
        }
        return;
    }

    if (row >= FROG_ROW_ROAD_START && row < FROG_ROW_ROAD_START + FROG_ROW_ROAD_COUNT) {
        int lane = frog_lane_for_row(row, g_road_rows, FROG_MAX_ROAD_LANES);
        int count;
        int i;
        int sprite_w = FROG_TILE;
        int sprite_h = FROG_TILE;

        if (lane < 0) {
            return;
        }

        count = g_params.road_count[lane];

        if (g_road_vehicle_type[lane] == 2) {
            sprite_w = frog_sprite_width(&g_truck1, FROG_TILE * 2);
            sprite_h = frog_sprite_height(&g_truck1, FROG_TILE);
        } else if (g_road_vehicle_type[lane] == 1) {
            sprite_w = frog_sprite_width(&g_car2, FROG_TILE * 2);
            sprite_h = frog_sprite_height(&g_car2, FROG_TILE);
        } else {
            sprite_w = frog_sprite_width(&g_car1, FROG_TILE * 2);
            sprite_h = frog_sprite_height(&g_car1, FROG_TILE);
        }

        for (i = 0; i < count; ++i) {
            float car_x = g_road_positions[lane].x[i];
            float car_y = (float)(FROG_GRID_Y + row * FROG_TILE);
            if (frog_rect_intersect(g_frog_x, g_frog_y, (float)frog_w, (float)frog_h,
                                    car_x, car_y, (float)sprite_w, (float)sprite_h)) {
                frog_kill("ATROPELLADO");
                return;
            }
        }
    }

    if (row >= FROG_ROW_RIVER_START && row < FROG_ROW_RIVER_START + FROG_ROW_RIVER_COUNT) {
        int lane = frog_lane_for_row(row, g_river_rows, FROG_MAX_RIVER_LANES);
        int count;
        int i;
        int on_platform = 0;
        float carry_speed = 0.0f;

        if (lane < 0) {
            return;
        }

        count = g_params.river_count[lane];

        for (i = 0; i < count; ++i) {
            float plat_x = g_river_positions[lane].x[i];
            float plat_y = (float)(FROG_GRID_Y + row * FROG_TILE);
            float plat_w = (float)FROG_TILE;
            float plat_h = (float)FROG_TILE;

            if (g_river_platform_type[lane] == FROG_PLATFORM_LOG) {
                plat_w = (float)(g_log_length[lane] * FROG_TILE);
                plat_h = (float)FROG_TILE;
            } else {
                plat_w = (float)frog_sprite_width(&g_turtle1, FROG_TILE);
                plat_h = (float)frog_sprite_height(&g_turtle1, FROG_TILE);
            }

            if (frog_rect_intersect(g_frog_x, g_frog_y, (float)frog_w, (float)frog_h,
                                    plat_x, plat_y, plat_w, plat_h)) {
                on_platform = 1;
                carry_speed = g_params.river_speed[lane] * g_settings.speed_multiplier * g_river_direction[lane];
                break;
            }
        }

        if (!on_platform) {
            frog_kill("AHOGADO");
            return;
        }

        g_frog_x += carry_speed;
        if (g_frog_x < 0.0f || g_frog_x > (float)(VIDEO_WIDTH - frog_w)) {
            frog_kill("AHOGADO");
            return;
        }
    }
}

void Frog_Update(void)
{
    if (g_finished) {
        return;
    }

    sound_update();

    g_frame_counter++;
    g_water_scroll_acc++;
    if (g_water_scroll_acc >= FROG_WATER_SCROLL_FRAMES) {
        g_water_scroll_px = (g_water_scroll_px + 1) % FROG_WATER_PERIOD;
        g_water_scroll_acc = 0;
    }

    if (g_respawn_delay_ticks > 0) {
        g_respawn_delay_ticks--;
        return;
    }

    frog_handle_input();
    frog_update_lanes();

    if (g_timer_ticks > 0) {
        g_timer_ticks--;
        if (g_timer_ticks == 0) {
            frog_kill("TIEMPO");
            return;
        }
    }

    if (g_hop_ticks > 0) {
        g_hop_ticks--;
    }

    frog_handle_collisions();
}

static unsigned int frog_hash_u16(unsigned int x)
{
    // Hash barata de 16 bits, determinista y compatible con C89
    x ^= (x << 7);
    x ^= (x >> 9);
    x *= 40503u;      // Constante típica tipo LCG
    x ^= (x >> 8);
    return x;
}

static void frog_draw_water(uint8_t *backbuf, int x0, int y0, int w, int h)
{
    int y;
    (void)backbuf;

    v_fill_rect(x0, y0, w, h, FROG_COLOR_WATER_BLUE_MED);

    for (y = y0; y < y0 + h; ++y) {
        int local_y;
        unsigned int seed;
        int mode;
        int len, gap;
        int wobble;
        int row_offset;
        int start;
        int x;

        local_y = y - y0;

        seed = frog_hash_u16((unsigned int)(local_y * 997));

        // Variación poco periódica
        if ((seed & 15u) == 0u) {
            continue;
        }

        mode = (int)(seed & 3u);
        if (mode == 0) { len = 4; gap = 7; }
        else if (mode == 1) { len = 6; gap = 8; }
        else if (mode == 2) { len = 8; gap = 10; }
        else { len = 5; gap = 6; }

        wobble = (int)((seed >> 8) & 7u); // 0..7
        row_offset = (local_y * 3 + wobble) % FROG_WATER_PERIOD;
        start = (row_offset + g_water_scroll_px) % FROG_WATER_PERIOD;

        for (x = -FROG_WATER_PERIOD + start; x < w; x += (len + gap)) {
            int seg_x, seg_w, clip_left, clip_right;

            seg_x = x0 + x;
            seg_w = len;

            clip_left  = x0 - seg_x;
            clip_right = (seg_x + seg_w) - (x0 + w);

            if (clip_left > 0) { seg_x += clip_left; seg_w -= clip_left; }
            if (clip_right > 0) { seg_w -= clip_right; }

            if (seg_w > 0) {
                v_fill_rect(seg_x, y, seg_w, 1, FROG_COLOR_WATER_BLUE_DARK);
            }
        }
    }
}


static void frog_draw_stage_background(uint8_t *backbuf)
{
    const int grid_top = FROG_GRID_Y;
    const int grid_height = FROG_GRID_ROWS * FROG_TILE;
    const int goal_y = grid_top + FROG_ROW_GOAL * FROG_TILE;
    const int river_y = grid_top + FROG_ROW_RIVER_START * FROG_TILE;
    const int river_h = FROG_ROW_RIVER_COUNT * FROG_TILE;
    const int median_y = grid_top + FROG_ROW_MEDIAN * FROG_TILE;
    const int road_y = grid_top + FROG_ROW_ROAD_START * FROG_TILE;
    const int road_h = FROG_ROW_ROAD_COUNT * FROG_TILE;
    const int start_y = grid_top + FROG_ROW_START_BOTTOM * FROG_TILE;
    int y;

    (void)backbuf;

    v_clear(FROG_COLOR_SEPARATOR);
    v_fill_rect(0, grid_top, VIDEO_WIDTH, grid_height, FROG_COLOR_SEPARATOR);

    v_fill_rect(0, goal_y, VIDEO_WIDTH, FROG_TILE, FROG_COLOR_SAFE_ZONE);
    frog_draw_water(backbuf, 0, river_y, VIDEO_WIDTH, river_h);

    v_fill_rect(0, river_y - 1, VIDEO_WIDTH, 1, FROG_COLOR_SEPARATOR);

    v_fill_rect(0, median_y, VIDEO_WIDTH, FROG_TILE, FROG_COLOR_SAFE_ZONE);
    v_fill_rect(0, median_y, VIDEO_WIDTH, 1, FROG_COLOR_SEPARATOR);
    v_fill_rect(0, median_y + FROG_TILE - 1, VIDEO_WIDTH, 1, FROG_COLOR_SEPARATOR);

    v_fill_rect(0, road_y, VIDEO_WIDTH, road_h, FROG_COLOR_ROAD_GRAY_MED);
    v_fill_rect(0, road_y + road_h - 1, VIDEO_WIDTH, 1, FROG_COLOR_ROAD_GRAY_DARK);

    for (y = 1; y < FROG_ROW_ROAD_COUNT; ++y) {
        int line_y = road_y + (y * FROG_TILE);
        int x;
        for (x = 0; x < VIDEO_WIDTH; x += 16) {
            v_fill_rect(x, line_y, 8, 1, FROG_COLOR_LANE_WHITE);
        }
    }

    v_fill_rect(0, start_y, VIDEO_WIDTH, FROG_TILE, FROG_COLOR_SAFE_ZONE);
    v_fill_rect(0, start_y - 1, VIDEO_WIDTH, 1, FROG_COLOR_SEPARATOR);
}

static void frog_draw_goal_slots(void)
{
    int i;
    int y = FROG_GRID_Y + FROG_ROW_GOAL * FROG_TILE;

    for (i = 0; i < FROG_GOAL_SLOTS; ++i) {
        int x = g_goal_slots[i] * FROG_TILE;
        frog_blit_sprite(x, y, &g_lilly1);
    }
}

static void frog_draw_logs(float x, int row, int length)
{
    int i;
    int y = FROG_GRID_Y + row * FROG_TILE;
    int start_x = (int)x;

    if (length <= 0) {
        return;
    }

    if (length == 1) {
        frog_blit_sprite(start_x, y, &g_tree1);
        return;
    }

    frog_blit_sprite(start_x, y, &g_tree1);
    for (i = 1; i < length - 1; ++i) {
        frog_blit_sprite(start_x + i * FROG_TILE, y, &g_tree2);
    }
    if (g_tree3.w != 0 && g_tree3.h != 0) {
        frog_blit_sprite(start_x + (length - 1) * FROG_TILE, y, &g_tree3);
    } else if (g_tree1.w != 0 && g_tree1.h != 0) {
        frog_blit_sprite_flipped(start_x + (length - 1) * FROG_TILE, y, &g_tree1, 1, 0);
    } else {
        frog_blit_sprite(start_x + (length - 1) * FROG_TILE, y, &g_tree2);
    }
}

static void frog_draw_vehicles(float alpha)
{
    int lane;
    int i;

    for (lane = 0; lane < FROG_MAX_ROAD_LANES; ++lane) {
        int count = g_params.road_count[lane];
        int row = g_road_rows[lane];
        int y = FROG_GRID_Y + row * FROG_TILE;
        const FrogSprite *sprite = &g_car1;
        int flip = (g_lane_direction[lane] < 0);

        if (g_road_vehicle_type[lane] == 2) {
            sprite = &g_truck1;
        } else if (g_road_vehicle_type[lane] == 1) {
            sprite = &g_car2;
        }

        for (i = 0; i < count; ++i) {
            float x = g_road_positions[lane].x_prev[i] +
                (g_road_positions[lane].x[i] - g_road_positions[lane].x_prev[i]) * alpha;
            if (flip) {
                frog_blit_sprite_flipped((int)x, y, sprite, 1, 0);
            } else {
                frog_blit_sprite((int)x, y, sprite);
            }
        }
    }
}

static void frog_draw_platforms(float alpha)
{
    int lane;
    int i;

    for (lane = 0; lane < FROG_MAX_RIVER_LANES; ++lane) {
        int count = g_params.river_count[lane];
        int row = g_river_rows[lane];

        for (i = 0; i < count; ++i) {
            float x = g_river_positions[lane].x_prev[i] +
                (g_river_positions[lane].x[i] - g_river_positions[lane].x_prev[i]) * alpha;
            if (g_river_platform_type[lane] == FROG_PLATFORM_LOG) {
                frog_draw_logs(x, row, g_log_length[lane]);
            } else {
                int y = FROG_GRID_Y + row * FROG_TILE;
                frog_blit_sprite((int)x, y, &g_turtle1);
            }
        }
    }
}

static void frog_draw_frog(float alpha)
{
    float x = g_frog_x_prev + (g_frog_x - g_frog_x_prev) * alpha;
    float y = g_frog_y_prev + (g_frog_y - g_frog_y_prev) * alpha;
    const FrogSprite *sprite = (g_hop_ticks > 0) ? &g_frog2 : &g_frog1;

    if (g_frog_dir == FROG_DIR_LEFT) {
        frog_blit_sprite_rotated_45((int)x, (int)y, sprite, 0);
    } else if (g_frog_dir == FROG_DIR_RIGHT) {
        frog_blit_sprite_rotated_45((int)x, (int)y, sprite, 1);
    } else if (g_frog_dir == FROG_DIR_DOWN) {
        frog_blit_sprite_flipped((int)x, (int)y, sprite, 0, 1);
    } else {
        frog_blit_sprite((int)x, (int)y, sprite);
    }
}

void Frog_DrawInterpolated(float alpha)
{
    char hud[64];
    char score[24];
    int timer_seconds = g_timer_ticks / FROG_TICKS_PER_SECOND;

    frog_draw_stage_background(NULL);
    frog_draw_goal_slots();
    frog_draw_vehicles(alpha);
    frog_draw_platforms(alpha);
    frog_draw_frog(alpha);

    snprintf(hud, sizeof(hud), "D:%s x%.2f", difficulty_short(g_settings.difficulty), g_settings.speed_multiplier);
    v_puts(0, 0, "1981", 7);
    v_puts(VIDEO_WIDTH - (text_len(hud) * 8), 0, hud, 7);

    snprintf(score, sizeof(score), "TIEMPO %02d  %07llu", timer_seconds, (unsigned long long)g_score);
    {
        int score_x = (VIDEO_WIDTH - (text_len(score) * 8)) / 2 - 16;
        if (score_x < 0) {
            score_x = 0;
        }
        v_puts(score_x, 0, score, 15);
    }

    snprintf(hud, sizeof(hud), "VIDAS %d", g_lives);
    v_puts(VIDEO_WIDTH - (text_len(hud) * 8), VIDEO_HEIGHT - 8, hud, 15);

    v_present();
}

void Frog_End(void)
{
    if (g_did_win) {
        char score_text[16];
        uint64_t time_bonus = (uint64_t)(g_timer_ticks * FROG_SCORE_TIME_BONUS / (g_params.timer_seconds * FROG_TICKS_PER_SECOND));
        if (time_bonus > FROG_SCORE_TIME_BONUS) {
            time_bonus = FROG_SCORE_TIME_BONUS;
        }
        g_final_score = g_score + time_bonus;
        high_scores_format_score(score_text, sizeof(score_text), g_final_score);
        snprintf(g_end_detail, sizeof(g_end_detail), "PUNTOS %s", score_text);
    } else {
        if (g_last_death_reason[0] != '\0') {
            snprintf(g_end_detail, sizeof(g_end_detail), "%s", g_last_death_reason);
        } else {
            snprintf(g_end_detail, sizeof(g_end_detail), "SIN VIDAS");
        }
    }

    frog_free_sprites();
}

int Frog_IsFinished(void)
{
    return g_finished;
}

int Frog_DidWin(void)
{
    return g_did_win;
}

const char *Frog_GetEndDetail(void)
{
    return g_end_detail;
}

uint64_t Frog_GetScore(void)
{
    return g_final_score;
}
