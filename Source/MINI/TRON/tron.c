#include "tron.h"

#include "../../CORE/input.h"
#include "../../CORE/options.h"
#include "../../CORE/sound.h"
#include "../../CORE/video.h"
#include "../../CORE/keyboard.h"
#include "../../CORE/sprite_dat.h"
#include "../../CORE/high_scores.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#define TRON_FAST_RENDER 1

#define TRON_CELL_SIZE 8
#define TRON_GRID_COLS 40
#define TRON_GRID_ROWS 24
#define TRON_GRID_ORIGIN_X 0
#define TRON_GRID_ORIGIN_Y 8
#define TRON_TICKS_PER_SECOND 60
#define TRON_MAX_SPRITE_PIXELS 4096

#define TRON_COLOR_BG 0
#define TRON_COLOR_BORDER 207
#define TRON_COLOR_GRID 1
#define TRON_COLOR_PLAYER_TRAIL 177
#define TRON_COLOR_ENEMY_TRAIL 20
#define TRON_COLOR_HUD 15
#define TRON_COLOR_EXPLOSION_0 53
#define TRON_COLOR_EXPLOSION_1 35

#define TRON_EXPLOSION_PARTICLES 16
#define TRON_EXPLOSION_LIFE 18
#define TRON_FINISH_DELAY_TICKS 24

typedef enum {
    TRON_CELL_EMPTY = 0,
    TRON_CELL_WALL,
    TRON_CELL_PLAYER_TRAIL,
    TRON_CELL_ENEMY_TRAIL
} TronCell;

typedef enum {
    TRON_DIR_UP = 0,
    TRON_DIR_RIGHT,
    TRON_DIR_DOWN,
    TRON_DIR_LEFT
} TronDir;

typedef struct {
    float move_speed;
    int allow_reverse;
    int ai_mistake_chance;
    int ai_aggression;
    int ai_lookahead;
} TronParams;

typedef struct {
    unsigned short w;
    unsigned short h;
    unsigned char far *pixels;
} TronSprite;

typedef struct {
    int active;
    float x;
    float y;
    float vx;
    float vy;
    int life;
} TronParticle;

static GameSettings g_settings;
static TronParams g_params;
static TronSprite g_bike_player;
static TronSprite g_bike_enemy;
static int g_sprites_loaded = 0;
#if TRON_FAST_RENDER
static TronSprite g_bike_player_rot[4];
static TronSprite g_bike_enemy_rot[4];
#endif

static TronCell g_grid[TRON_GRID_ROWS][TRON_GRID_COLS];
static int far g_open_area_queue_x[TRON_GRID_COLS * TRON_GRID_ROWS];
static int far g_open_area_queue_y[TRON_GRID_COLS * TRON_GRID_ROWS];
static unsigned char far g_open_area_visited[TRON_GRID_ROWS][TRON_GRID_COLS];
static unsigned char far *g_arena_layer = NULL;
static int g_arena_ready = 0;

static float g_player_x = 0.0f;
static float g_player_y = 0.0f;
static float g_player_prev_x = 0.0f;
static float g_player_prev_y = 0.0f;
static float g_enemy_x = 0.0f;
static float g_enemy_y = 0.0f;
static float g_enemy_prev_x = 0.0f;
static float g_enemy_prev_y = 0.0f;
static TronDir g_player_dir = TRON_DIR_RIGHT;
static TronDir g_player_next_dir = TRON_DIR_RIGHT;
static TronDir g_enemy_dir = TRON_DIR_LEFT;

static int g_move_timer = 0;
static int g_move_interval = 0;
static int g_elapsed_ticks = 0;
static int g_finished = 0;
static int g_did_win = 0;
static int g_use_keyboard = 1;
static int g_finish_delay = 0;
static uint64_t g_final_score = 0;
static char g_end_detail[32] = "";

static TronParticle g_particles[TRON_EXPLOSION_PARTICLES];

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

static void tron_select_params(unsigned char difficulty, TronParams *params)
{
    if (!params) {
        return;
    }

    switch (difficulty) {
    case DIFFICULTY_EASY:
        params->move_speed = 4.0f;
        params->allow_reverse = 1;
        params->ai_mistake_chance = 30;
        params->ai_aggression = 20;
        params->ai_lookahead = 8;
        break;
    case DIFFICULTY_HARD:
        params->move_speed = 8.0f;
        params->allow_reverse = 0;
        params->ai_mistake_chance = 4;
        params->ai_aggression = 95;
        params->ai_lookahead = 16;
        break;
    case DIFFICULTY_NORMAL:
    default:
        params->move_speed = 6.0f;
        params->allow_reverse = 0;
        params->ai_mistake_chance = 12;
        params->ai_aggression = 55;
        params->ai_lookahead = 12;
        break;
    }
}

static int tron_cell_in_bounds(int x, int y)
{
    return x >= 0 && x < TRON_GRID_COLS && y >= 0 && y < TRON_GRID_ROWS;
}

static int tron_cell_blocked(int x, int y)
{
    if (!tron_cell_in_bounds(x, y)) {
        return 1;
    }
    return g_grid[y][x] != TRON_CELL_EMPTY;
}

static void tron_dir_to_delta(TronDir dir, int *dx, int *dy)
{
    switch (dir) {
    case TRON_DIR_UP:
        *dx = 0;
        *dy = -1;
        break;
    case TRON_DIR_RIGHT:
        *dx = 1;
        *dy = 0;
        break;
    case TRON_DIR_DOWN:
        *dx = 0;
        *dy = 1;
        break;
    case TRON_DIR_LEFT:
    default:
        *dx = -1;
        *dy = 0;
        break;
    }
}

static TronDir tron_dir_opposite(TronDir dir)
{
    switch (dir) {
    case TRON_DIR_UP:
        return TRON_DIR_DOWN;
    case TRON_DIR_RIGHT:
        return TRON_DIR_LEFT;
    case TRON_DIR_DOWN:
        return TRON_DIR_UP;
    case TRON_DIR_LEFT:
    default:
        return TRON_DIR_RIGHT;
    }
}

static int tron_free_run(int x, int y, int dx, int dy)
{
    int steps = 0;
    int nx = x;
    int ny = y;

    while (1) {
        nx += dx;
        ny += dy;
        if (tron_cell_blocked(nx, ny)) {
            break;
        }
        steps++;
        if (steps > TRON_GRID_COLS + TRON_GRID_ROWS) {
            break;
        }
    }

    return steps;
}

static int tron_count_open_area(int start_x, int start_y, int max_cells)
{
    int head = 0;
    int tail = 0;
    int count = 0;

    if (tron_cell_blocked(start_x, start_y)) {
        return 0;
    }

    memset(g_open_area_visited, 0, sizeof(g_open_area_visited));
    g_open_area_visited[start_y][start_x] = 1;
    g_open_area_queue_x[tail] = start_x;
    g_open_area_queue_y[tail] = start_y;
    tail++;

    while (head < tail && count < max_cells) {
        int cx = g_open_area_queue_x[head];
        int cy = g_open_area_queue_y[head];
        int dir;
        head++;
        count++;

        for (dir = 0; dir < 4; ++dir) {
            int dx = 0;
            int dy = 0;
            int nx;
            int ny;
            tron_dir_to_delta((TronDir)dir, &dx, &dy);
            nx = cx + dx;
            ny = cy + dy;
            if (!tron_cell_in_bounds(nx, ny)) {
                continue;
            }
            if (g_open_area_visited[ny][nx]) {
                continue;
            }
            if (g_grid[ny][nx] != TRON_CELL_EMPTY) {
                continue;
            }
            g_open_area_visited[ny][nx] = 1;
            g_open_area_queue_x[tail] = nx;
            g_open_area_queue_y[tail] = ny;
            tail++;
        }
    }

    return count;
}

static int tron_clear_line(int x0, int y0, int x1, int y1)
{
    if (x0 == x1) {
        int y = y0;
        int dy = (y1 > y0) ? 1 : -1;
        while (y != y1) {
            if (tron_cell_blocked(x0, y)) {
                return 0;
            }
            y += dy;
        }
        return 1;
    }
    if (y0 == y1) {
        int x = x0;
        int dx = (x1 > x0) ? 1 : -1;
        while (x != x1) {
            if (tron_cell_blocked(x, y0)) {
                return 0;
            }
            x += dx;
        }
        return 1;
    }

    return 0;
}

// Cuenta salidas libres para evitar jaulas
static int tron_count_exits(int x, int y)
{
    int exits = 0;
    int d;
    for (d = 0; d < 4; ++d) {
        int dx = 0, dy = 0;
        tron_dir_to_delta((TronDir)d, &dx, &dy);
        if (!tron_cell_blocked(x + dx, y + dy)) {
            exits++;
        }
    }
    return exits;
}

// Distancia mínima al borde (más grande = más seguro)
static int tron_edge_dist(int x, int y)
{
    int e = x;
    int t = TRON_GRID_COLS - 1 - x;
    int u = y;
    int b = TRON_GRID_ROWS - 1 - y;

    if (t < e) {
        e = t;
    }
    if (u < e) {
        e = u;
    }
    if (b < e) {
        e = b;
    }
    return e;
}

static TronDir tron_ai_pick_dir(void)
{
    TronDir candidates[4];
    float scores[4];
    int count = 0;
    int best = -1;
    float best_score = -100000.0f;
    int dir;
    int ex = (int)g_enemy_x;
    int ey = (int)g_enemy_y;
    int px = (int)g_player_x;
    int py = (int)g_player_y;
    int reverse_block = tron_dir_opposite(g_enemy_dir);

    for (dir = 0; dir < 4; ++dir) {
        TronDir d = (TronDir)dir;
        int dx = 0;
        int dy = 0;
        int nx;
        int ny;
        tron_dir_to_delta(d, &dx, &dy);
        nx = ex + dx;
        ny = ey + dy;
        if (d == reverse_block) {
            continue;
        }
        if (tron_cell_blocked(nx, ny)) {
            continue;
        }
        candidates[count] = d;
        scores[count] = 0.0f;
        count++;
    }

    if (count == 0) {
        return g_enemy_dir;
    }

    if ((rand() % 100) < g_params.ai_mistake_chance) {
        return candidates[rand() % count];
    }

    for (dir = 0; dir < count; ++dir) {
        TronDir d = candidates[dir];
        int dx = 0;
        int dy = 0;
        int nx;
        int ny;
        int run;
        int area;
        int dist;
        int edge_dist;
        float score;

        tron_dir_to_delta(d, &dx, &dy);
        nx = ex + dx;
        ny = ey + dy;

        run = tron_free_run(ex, ey, dx, dy);
        area = tron_count_open_area(nx, ny, g_params.ai_lookahead * g_params.ai_lookahead);
        dist = abs(px - nx) + abs(py - ny);
        edge_dist = nx;
        if ((TRON_GRID_COLS - 1 - nx) < edge_dist) {
            edge_dist = TRON_GRID_COLS - 1 - nx;
        }
        if (ny < edge_dist) {
            edge_dist = ny;
        }
        if ((TRON_GRID_ROWS - 1 - ny) < edge_dist) {
            edge_dist = TRON_GRID_ROWS - 1 - ny;
        }

        score = 0.0f;
        score += run * 2.0f;
        score -= dist * (0.4f + (g_params.ai_aggression / 100.0f));
        score += area * 0.15f;
        score -= edge_dist * (g_params.ai_aggression / 120.0f);

        if (tron_clear_line(nx, ny, px, py)) {
            score += (float)g_params.ai_aggression;
        }

        scores[dir] = score;
    }

    for (dir = 0; dir < count; ++dir) {
        if (scores[dir] > best_score) {
            best_score = scores[dir];
            best = dir;
        }
    }

    if (best < 0) {
        return candidates[0];
    }

    return candidates[best];
}

static float tron_ai_eval_pos(int nx, int ny, int px, int py)
{
    int run, area, dist, exits, edge;
    float score;

    run = 0;
    area = tron_count_open_area(nx, ny, g_params.ai_lookahead * g_params.ai_lookahead);
    dist = abs(px - nx) + abs(py - ny);
    exits = tron_count_exits(nx, ny);
    edge = tron_edge_dist(nx, ny);

    score = 0.0f;

    score += area * 0.20f;

    if (exits <= 1) {
        score -= 80.0f;
    } else if (exits == 2) {
        score -= 18.0f;
    } else {
        score += 10.0f;
    }

    score += edge * 0.8f;

    score -= dist * (0.18f + (g_params.ai_aggression / 220.0f));

    if (tron_clear_line(nx, ny, px, py)) {
        score += (float)g_params.ai_aggression * 0.55f;
    }

    return score;
}

static TronDir tron_ai_pick_dir_smart(void)
{
    TronDir best_dir = g_enemy_dir;
    float best_score = -1000000.0f;

    int ex = (int)g_enemy_x;
    int ey = (int)g_enemy_y;
    int px = (int)g_player_x;
    int py = (int)g_player_y;

    TronDir reverse_block = tron_dir_opposite(g_enemy_dir);

    if ((rand() % 100) < g_params.ai_mistake_chance) {
        TronDir opts[3];
        int n = 0, dir;
        for (dir = 0; dir < 4; ++dir) {
            TronDir d = (TronDir)dir;
            int dx = 0, dy = 0, nx, ny;
            if (d == reverse_block) {
                continue;
            }
            tron_dir_to_delta(d, &dx, &dy);
            nx = ex + dx;
            ny = ey + dy;
            if (!tron_cell_blocked(nx, ny)) {
                opts[n++] = d;
            }
        }
        if (n > 0) {
            return opts[rand() % n];
        }
        return g_enemy_dir;
    }

    {
        int dir;
        for (dir = 0; dir < 4; ++dir) {
            TronDir d = (TronDir)dir;
            int dx = 0, dy = 0;
            int nx, ny;
            int run;
            float s1, s2_best, score;

            if (d == reverse_block) {
                continue;
            }
            tron_dir_to_delta(d, &dx, &dy);
            nx = ex + dx;
            ny = ey + dy;
            if (tron_cell_blocked(nx, ny)) {
                continue;
            }

            run = tron_free_run(ex, ey, dx, dy);

            s1 = tron_ai_eval_pos(nx, ny, px, py);
            s1 += run * 2.2f;

            s2_best = -1000000.0f;
            {
                int dir2;
                TronDir reverse2 = tron_dir_opposite(d);
                for (dir2 = 0; dir2 < 4; ++dir2) {
                    TronDir d2 = (TronDir)dir2;
                    int dx2 = 0, dy2 = 0, nx2, ny2;
                    float s2;

                    if (d2 == reverse2) {
                        continue;
                    }
                    tron_dir_to_delta(d2, &dx2, &dy2);
                    nx2 = nx + dx2;
                    ny2 = ny + dy2;
                    if (tron_cell_blocked(nx2, ny2)) {
                        continue;
                    }

                    s2 = tron_ai_eval_pos(nx2, ny2, px, py);
                    if (s2 > s2_best) {
                        s2_best = s2;
                    }
                }
            }

            if (s2_best < -999999.0f) {
                s2_best = -120.0f;
            }

            {
                float future_blend = 0.65f;
                if (g_settings.difficulty == DIFFICULTY_EASY) {
                    future_blend = 0.45f;
                }
                if (g_settings.difficulty == DIFFICULTY_HARD) {
                    future_blend = 0.80f;
                }

                score = s1 + future_blend * s2_best;
            }

            if (score > best_score) {
                best_score = score;
                best_dir = d;
            }
        }
    }

    return best_dir;
}

static void tron_reset_grid(void)
{
    int x;
    int y;

    g_arena_ready = 0;

    for (y = 0; y < TRON_GRID_ROWS; ++y) {
        for (x = 0; x < TRON_GRID_COLS; ++x) {
            g_grid[y][x] = TRON_CELL_EMPTY;
        }
    }

    for (x = 0; x < TRON_GRID_COLS; ++x) {
        g_grid[0][x] = TRON_CELL_WALL;
        g_grid[TRON_GRID_ROWS - 1][x] = TRON_CELL_WALL;
    }
    for (y = 0; y < TRON_GRID_ROWS; ++y) {
        g_grid[y][0] = TRON_CELL_WALL;
        g_grid[y][TRON_GRID_COLS - 1] = TRON_CELL_WALL;
    }
}

static void tron_reset_particles(void)
{
    int i;
    for (i = 0; i < TRON_EXPLOSION_PARTICLES; ++i) {
        g_particles[i].active = 0;
    }
}

static void tron_spawn_explosion(float x, float y)
{
    int i;
    int slot = 0;
    const float speed = 1.1f;
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

    for (i = 0; i < TRON_EXPLOSION_PARTICLES; ++i) {
        int index = i % 8;
        for (; slot < TRON_EXPLOSION_PARTICLES; ++slot) {
            if (!g_particles[slot].active) {
                g_particles[slot].active = 1;
                g_particles[slot].x = x;
                g_particles[slot].y = y;
                g_particles[slot].vx = dirs[index][0] * speed;
                g_particles[slot].vy = dirs[index][1] * speed;
                g_particles[slot].life = TRON_EXPLOSION_LIFE;
                slot++;
                break;
            }
        }
    }
}

static void tron_update_particles(void)
{
    int i;
    for (i = 0; i < TRON_EXPLOSION_PARTICLES; ++i) {
        if (!g_particles[i].active) {
            continue;
        }
        g_particles[i].x += g_particles[i].vx;
        g_particles[i].y += g_particles[i].vy;
        g_particles[i].life--;
        if (g_particles[i].life <= 0) {
            g_particles[i].active = 0;
        }
    }
}

static int tron_alloc_sprite_pixels(TronSprite *sprite)
{
    if (!sprite) {
        return 0;
    }
    if (!sprite->pixels) {
        sprite->pixels = (unsigned char far *)_fmalloc(TRON_MAX_SPRITE_PIXELS);
        if (!sprite->pixels) {
            return 0;
        }
    }
    return 1;
}

static void tron_blit_sprite_rotated(int x, int y, const TronSprite *sprite, TronDir dir)
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
        for (sx = 0; sx < w; ++sx) {
            unsigned char color = sprite->pixels[sy * w + sx];
            int dx = sx;
            int dy = sy;

            if (color == 0) {
                continue;
            }

            switch (dir) {
            case TRON_DIR_RIGHT:
                dx = sx;
                dy = sy;
                break;
            case TRON_DIR_LEFT:
                dx = (w - 1) - sx;
                dy = (h - 1) - sy;
                break;
            case TRON_DIR_UP:
                dx = sy;
                dy = (w - 1) - sx;
                break;
            case TRON_DIR_DOWN:
            default:
                dx = (h - 1) - sy;
                dy = sx;
                break;
            }

            v_putpixel(x + dx, y + dy, color);
        }
    }
}

#if TRON_FAST_RENDER
static void tron_sprite_rotate_into(const TronSprite *src, TronSprite *dst, TronDir dir)
{
    int sx;
    int sy;
    int w;
    int h;

    if (!src || !dst) {
        return;
    }

    if (!tron_alloc_sprite_pixels(dst)) {
        return;
    }

    w = src->w;
    h = src->h;

    dst->w = (dir == TRON_DIR_UP || dir == TRON_DIR_DOWN) ? h : w;
    dst->h = (dir == TRON_DIR_UP || dir == TRON_DIR_DOWN) ? w : h;

    for (sy = 0; sy < (int)dst->h; ++sy) {
        for (sx = 0; sx < (int)dst->w; ++sx) {
            int ox = sx;
            int oy = sy;
            unsigned char p;

            switch (dir) {
            case TRON_DIR_RIGHT:
                ox = sx;
                oy = sy;
                break;
            case TRON_DIR_LEFT:
                ox = (w - 1) - sx;
                oy = (h - 1) - sy;
                break;
            case TRON_DIR_UP:
                ox = (w - 1) - sy;
                oy = sx;
                break;
            case TRON_DIR_DOWN:
            default:
                ox = sy;
                oy = (h - 1) - sx;
                break;
            }

            if (ox < 0 || oy < 0 || ox >= w || oy >= h) {
                p = 0;
            } else {
                p = src->pixels[oy * w + ox];
            }
            dst->pixels[sy * dst->w + sx] = p;
        }
    }
}

static void tron_build_rotated_sprites_once(void)
{
    int d;

    for (d = 0; d < 4; ++d) {
        tron_sprite_rotate_into(&g_bike_player, &g_bike_player_rot[d], (TronDir)d);
        tron_sprite_rotate_into(&g_bike_enemy, &g_bike_enemy_rot[d], (TronDir)d);
    }
}
#endif

static void buf_putpixel(unsigned char far *buf, int x, int y, unsigned char c)
{
    if (!buf) {
        return;
    }
    if (x < 0 || x >= VIDEO_WIDTH || y < 0 || y >= VIDEO_HEIGHT) {
        return;
    }
    buf[(unsigned long)y * VIDEO_WIDTH + x] = c;
}

static void buf_fill_rect(unsigned char far *buf, int x, int y, int w, int h, unsigned char c)
{
    int iy;
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    if (!buf || w <= 0 || h <= 0) {
        return;
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > VIDEO_WIDTH) {
        x1 = VIDEO_WIDTH;
    }
    if (y1 > VIDEO_HEIGHT) {
        y1 = VIDEO_HEIGHT;
    }
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (iy = y0; iy < y1; ++iy) {
        memset((void far *)(buf + ((unsigned long)iy * VIDEO_WIDTH) + x0), c, (size_t)(x1 - x0));
    }
}

static void buf_draw_dotted_rect(unsigned char far *buf, int x, int y, int w, int h,
                                 unsigned char base_color, unsigned char dot_color, int horizontal)
{
    int ix;
    int iy;
    const int dot_step = 6;
    const int dot_size = 2;
    const int phase = -2;

    for (iy = 0; iy < h; ++iy) {
        for (ix = 0; ix < w; ++ix) {
            buf_putpixel(buf, x + ix, y + iy, base_color);
        }
    }

    if (!horizontal) {
        for (iy = y; iy < y + h; ++iy) {
            int row = iy / dot_step;
            int x_offset = (row % 2) * (dot_step / 2);

            if ((iy % dot_step) != 0) {
                continue;
            }

            for (ix = x; ix < x + w; ++ix) {
                int xa = ix + x_offset + phase;
                if ((xa % dot_step) != 0) {
                    continue;
                }

                {
                    int remaining_w = (x + w) - ix;
                    int remaining_h = (y + h) - iy;
                    int dot_w = remaining_w >= dot_size ? dot_size : remaining_w;
                    int dot_h = remaining_h >= dot_size ? dot_size : remaining_h;
                    int dx;
                    int dy;

                    for (dy = 0; dy < dot_h; ++dy) {
                        for (dx = 0; dx < dot_w; ++dx) {
                            buf_putpixel(buf, ix + dx, iy + dy, dot_color);
                        }
                    }
                }
            }
        }
    } else {
        for (ix = x; ix < x + w; ++ix) {
            int col = ix / dot_step;
            int y_offset = (col % 2) * (dot_step / 2);

            if ((ix % dot_step) != 0) {
                continue;
            }

            for (iy = y; iy < y + h; ++iy) {
                int ya = iy + y_offset + phase;
                if ((ya % dot_step) != 0) {
                    continue;
                }

                {
                    int remaining_w = (x + w) - ix;
                    int remaining_h = (y + h) - iy;
                    int dot_w = remaining_w >= dot_size ? dot_size : remaining_w;
                    int dot_h = remaining_h >= dot_size ? dot_size : remaining_h;
                    int dx;
                    int dy;

                    for (dy = 0; dy < dot_h; ++dy) {
                        for (dx = 0; dx < dot_w; ++dx) {
                            buf_putpixel(buf, ix + dx, iy + dy, dot_color);
                        }
                    }
                }
            }
        }
    }
}

static void tron_build_arena_layer(void)
{
    int x;
    int y;

    if (!g_arena_layer) {
        g_arena_layer = (unsigned char far *)_fmalloc((size_t)VIDEO_WIDTH * VIDEO_HEIGHT);
    }
    if (!g_arena_layer) {
        return;
    }

    buf_fill_rect(g_arena_layer, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT, TRON_COLOR_BG);

    buf_fill_rect(g_arena_layer, TRON_GRID_ORIGIN_X, TRON_GRID_ORIGIN_Y,
                  TRON_GRID_COLS * TRON_CELL_SIZE, TRON_GRID_ROWS * TRON_CELL_SIZE,
                  TRON_COLOR_BG);

    buf_draw_dotted_rect(g_arena_layer,
                     TRON_GRID_ORIGIN_X, TRON_GRID_ORIGIN_Y,
                     TRON_GRID_COLS * TRON_CELL_SIZE, TRON_GRID_ROWS * TRON_CELL_SIZE,
                     TRON_COLOR_BG, TRON_COLOR_GRID, 1);

    for (y = 0; y < TRON_GRID_ROWS; ++y) {
        for (x = 0; x < TRON_GRID_COLS; ++x) {
            if (g_grid[y][x] == TRON_CELL_WALL) {
                buf_fill_rect(
                    g_arena_layer,
                    TRON_GRID_ORIGIN_X + x * TRON_CELL_SIZE,
                    TRON_GRID_ORIGIN_Y + y * TRON_CELL_SIZE,
                    TRON_CELL_SIZE,
                    TRON_CELL_SIZE,
                    TRON_COLOR_BORDER
                );
            }
        }
    }

    g_arena_ready = 1;
}

static void tron_update_input(void)
{
    TronDir desired = g_player_next_dir;

    if (g_use_keyboard) {
        if (kb_down(SC_UP)) {
            desired = TRON_DIR_UP;
        } else if (kb_down(SC_DOWN)) {
            desired = TRON_DIR_DOWN;
        } else if (kb_down(SC_LEFT)) {
            desired = TRON_DIR_LEFT;
        } else if (kb_down(SC_RIGHT)) {
            desired = TRON_DIR_RIGHT;
        }
    } else {
        int dx = 0;
        int dy = 0;
        unsigned char buttons = 0;

        if (in_joystick_direction(&dx, &dy, &buttons)) {
            if (abs(dx) > abs(dy)) {
                desired = (dx > 0) ? TRON_DIR_RIGHT : TRON_DIR_LEFT;
            } else if (dy != 0) {
                desired = (dy > 0) ? TRON_DIR_DOWN : TRON_DIR_UP;
            }
        }
    }

    g_player_next_dir = desired;
}

static TronDir tron_apply_player_dir(void)
{
    TronDir desired = g_player_next_dir;

    if (!g_params.allow_reverse && desired == tron_dir_opposite(g_player_dir)) {
        return g_player_dir;
    }

    return desired;
}

static void tron_set_start_positions(void)
{
    switch (g_settings.difficulty) {
    case DIFFICULTY_EASY:
        g_player_x = 6.0f;
        g_player_y = 12.0f;
        g_enemy_x = 33.0f;
        g_enemy_y = 12.0f;
        g_player_dir = TRON_DIR_RIGHT;
        g_enemy_dir = TRON_DIR_LEFT;
        break;
    case DIFFICULTY_HARD:
        g_player_x = 8.0f;
        g_player_y = 12.0f;
        g_enemy_x = 30.0f;
        g_enemy_y = 12.0f;
        g_player_dir = TRON_DIR_RIGHT;
        g_enemy_dir = TRON_DIR_LEFT;
        break;
    case DIFFICULTY_NORMAL:
    default:
        g_player_x = 7.0f;
        g_player_y = 12.0f;
        g_enemy_x = 31.0f;
        g_enemy_y = 12.0f;
        g_player_dir = TRON_DIR_RIGHT;
        g_enemy_dir = TRON_DIR_LEFT;
        break;
    }

    g_player_next_dir = g_player_dir;
}

static void tron_load_sprite(const char *path, TronSprite *sprite)
{
    if (!sprite) {
        return;
    }

    sprite->w = 0;
    sprite->h = 0;

    if (!tron_alloc_sprite_pixels(sprite)) {
        return;
    }

    sprite_dat_load_auto(path, &sprite->w, &sprite->h, sprite->pixels, TRON_MAX_SPRITE_PIXELS);
}

static void tron_free_sprite(TronSprite *sprite)
{
    if (!sprite || !sprite->pixels) {
        return;
    }

    _ffree(sprite->pixels);
    sprite->pixels = NULL;
    sprite->w = 0;
    sprite->h = 0;
}

static void tron_free_sprites(void)
{
    int d;

    if (!g_sprites_loaded) {
        return;
    }

    tron_free_sprite(&g_bike_player);
    tron_free_sprite(&g_bike_enemy);
#if TRON_FAST_RENDER
    for (d = 0; d < 4; ++d) {
        tron_free_sprite(&g_bike_player_rot[d]);
        tron_free_sprite(&g_bike_enemy_rot[d]);
    }
#else
    (void)d;
#endif

    g_sprites_loaded = 0;
}

void Tron_Init(const GameSettings *settings)
{
    if (settings) {
        g_settings = *settings;
    } else {
        memset(&g_settings, 0, sizeof(g_settings));
    }

    tron_select_params(g_settings.difficulty, &g_params);
    g_params.move_speed *= g_settings.speed_multiplier;

    if (g_params.move_speed < 2.0f) {
        g_params.move_speed = 2.0f;
    }

    g_move_interval = (int)((float)TRON_TICKS_PER_SECOND / g_params.move_speed);
    if (g_move_interval < 1) {
        g_move_interval = 1;
    }

    if (!g_sprites_loaded) {
        tron_load_sprite("SPRITES\\bike1.dat", &g_bike_player);
        tron_load_sprite("SPRITES\\bike2.dat", &g_bike_enemy);
        g_sprites_loaded = 1;
#if TRON_FAST_RENDER
        tron_build_rotated_sprites_once();
#endif
    }

    tron_reset_grid();
    tron_reset_particles();
    tron_build_arena_layer();
    tron_set_start_positions();

    g_player_prev_x = g_player_x;
    g_player_prev_y = g_player_y;
    g_enemy_prev_x = g_enemy_x;
    g_enemy_prev_y = g_enemy_y;

    g_move_timer = 0;
    g_elapsed_ticks = 0;
    g_finished = 0;
    g_did_win = 0;
    g_finish_delay = 0;
    g_final_score = 0;
    g_end_detail[0] = '\0';

    g_use_keyboard = 1;
    if (g_settings.input_mode == INPUT_JOYSTICK) {
        if (in_joystick_available()) {
            g_use_keyboard = 0;
        }
    }
}

void Tron_StorePreviousState(void)
{
    g_player_prev_x = g_player_x;
    g_player_prev_y = g_player_y;
    g_enemy_prev_x = g_enemy_x;
    g_enemy_prev_y = g_enemy_y;
}

void Tron_Update(void)
{
    if (g_finished) {
        return;
    }

    sound_update();
    tron_update_particles();

    if (g_finish_delay > 0) {
        g_finish_delay--;
        if (g_finish_delay <= 0) {
            g_finished = 1;
        }
        return;
    }

    g_elapsed_ticks++;
    g_final_score = (uint64_t)g_elapsed_ticks;
    tron_update_input();

    g_move_timer++;
    if (g_move_timer < g_move_interval) {
        return;
    }

    g_move_timer = 0;

    {
        TronDir next_player_dir = tron_apply_player_dir();
        TronDir next_enemy_dir = tron_ai_pick_dir_smart();
        int pdx = 0;
        int pdy = 0;
        int edx = 0;
        int edy = 0;
        int px = (int)g_player_x;
        int py = (int)g_player_y;
        int ex = (int)g_enemy_x;
        int ey = (int)g_enemy_y;
        int pnx;
        int pny;
        int enx;
        int eny;
        int player_collision = 0;
        int enemy_collision = 0;

        tron_dir_to_delta(next_player_dir, &pdx, &pdy);
        tron_dir_to_delta(next_enemy_dir, &edx, &edy);

        pnx = px + pdx;
        pny = py + pdy;
        enx = ex + edx;
        eny = ey + edy;

        if (tron_cell_blocked(pnx, pny)) {
            player_collision = 1;
        }

        if (tron_cell_blocked(enx, eny)) {
            enemy_collision = 1;
        }

        if ((pnx == enx && pny == eny) || (pnx == ex && pny == ey) || (enx == px && eny == py)) {
            player_collision = 1;
        }

        if (player_collision) {
            int spawn_x = clamp_int(pnx, 1, TRON_GRID_COLS - 2);
            int spawn_y = clamp_int(pny, 1, TRON_GRID_ROWS - 2);
            g_did_win = 0;
            g_finish_delay = TRON_FINISH_DELAY_TICKS;
            tron_spawn_explosion(TRON_GRID_ORIGIN_X + (spawn_x * TRON_CELL_SIZE) + 2,
                                 TRON_GRID_ORIGIN_Y + (spawn_y * TRON_CELL_SIZE) + 2);
            return;
        }

        if (enemy_collision) {
            int spawn_x = clamp_int(enx, 1, TRON_GRID_COLS - 2);
            int spawn_y = clamp_int(eny, 1, TRON_GRID_ROWS - 2);
            g_did_win = 1;
            g_finish_delay = TRON_FINISH_DELAY_TICKS;
            tron_spawn_explosion(TRON_GRID_ORIGIN_X + (spawn_x * TRON_CELL_SIZE) + 2,
                                 TRON_GRID_ORIGIN_Y + (spawn_y * TRON_CELL_SIZE) + 2);
            return;
        }

        g_grid[py][px] = TRON_CELL_PLAYER_TRAIL;
        g_grid[ey][ex] = TRON_CELL_ENEMY_TRAIL;

        if (g_arena_ready) {
            buf_fill_rect(g_arena_layer,
                          TRON_GRID_ORIGIN_X + (px * TRON_CELL_SIZE),
                          TRON_GRID_ORIGIN_Y + (py * TRON_CELL_SIZE),
                          TRON_CELL_SIZE, TRON_CELL_SIZE, TRON_COLOR_PLAYER_TRAIL);

            buf_fill_rect(g_arena_layer,
                          TRON_GRID_ORIGIN_X + (ex * TRON_CELL_SIZE),
                          TRON_GRID_ORIGIN_Y + (ey * TRON_CELL_SIZE),
                          TRON_CELL_SIZE, TRON_CELL_SIZE, TRON_COLOR_ENEMY_TRAIL);
        }

        g_player_dir = next_player_dir;
        g_enemy_dir = next_enemy_dir;
        g_player_x = (float)pnx;
        g_player_y = (float)pny;
        g_enemy_x = (float)enx;
        g_enemy_y = (float)eny;

    }

    return;
}

void Tron_DrawInterpolated(float alpha)
{
    int i;
    float px = g_player_prev_x + (g_player_x - g_player_prev_x) * alpha;
    float py = g_player_prev_y + (g_player_y - g_player_prev_y) * alpha;
    float ex = g_enemy_prev_x + (g_enemy_x - g_enemy_prev_x) * alpha;
    float ey = g_enemy_prev_y + (g_enemy_y - g_enemy_prev_y) * alpha;
    int player_px = TRON_GRID_ORIGIN_X + (int)(px * TRON_CELL_SIZE);
    int player_py = TRON_GRID_ORIGIN_Y + (int)(py * TRON_CELL_SIZE);
    int enemy_px = TRON_GRID_ORIGIN_X + (int)(ex * TRON_CELL_SIZE);
    int enemy_py = TRON_GRID_ORIGIN_Y + (int)(ey * TRON_CELL_SIZE);
    char hud[32];

#if TRON_FAST_RENDER
    if (g_arena_ready && g_arena_layer) {
        v_blit_fullscreen_fast((const unsigned char far *)g_arena_layer);
    } else {
        v_clear(TRON_COLOR_BG);
    }

    {
        TronSprite *sp = &g_bike_player_rot[(int)g_player_dir];
        TronSprite *se = &g_bike_enemy_rot[(int)g_enemy_dir];
        v_blit_sprite(player_px, player_py, sp->w, sp->h, (const unsigned char far *)sp->pixels, 0);
        v_blit_sprite(enemy_px, enemy_py, se->w, se->h, (const unsigned char far *)se->pixels, 0);
    }
#else
    if (g_arena_ready && g_arena_layer) {
        v_blit_sprite(0, 0, VIDEO_WIDTH, VIDEO_HEIGHT, (const unsigned char far *)g_arena_layer, 255);
    } else {
        v_clear(TRON_COLOR_BG);
    }

    // Dibuja borde de la arena
    {
        int x, y;

        for (y = 0; y < TRON_GRID_ROWS; ++y) {
            for (x = 0; x < TRON_GRID_COLS; ++x) {
                if (g_grid[y][x] == TRON_CELL_WALL) {
                    v_fill_rect(
                        TRON_GRID_ORIGIN_X + x * TRON_CELL_SIZE,
                        TRON_GRID_ORIGIN_Y + y * TRON_CELL_SIZE,
                        TRON_CELL_SIZE,
                        TRON_CELL_SIZE,
                        TRON_COLOR_BORDER
                    );
                }
            }
        }
    }

    tron_blit_sprite_rotated(player_px, player_py, &g_bike_player, g_player_dir);
    tron_blit_sprite_rotated(enemy_px, enemy_py, &g_bike_enemy, g_enemy_dir);
#endif

    for (i = 0; i < TRON_EXPLOSION_PARTICLES; ++i) {
        if (!g_particles[i].active) {
            continue;
        }
        {
            unsigned char color = TRON_COLOR_EXPLOSION_0;
            if (g_particles[i].life < (TRON_EXPLOSION_LIFE / 2)) {
                color = TRON_COLOR_EXPLOSION_1;
            }
            v_fill_rect((int)g_particles[i].x, (int)g_particles[i].y, 2, 2, color);
        }
    }

    snprintf(hud, sizeof(hud), "D:%s x%.2f", difficulty_short(g_settings.difficulty), g_settings.speed_multiplier);
    v_puts(0, 0, "1982", TRON_COLOR_HUD);
    v_puts(VIDEO_WIDTH - (int)(strlen(hud) * 8), 0, hud, TRON_COLOR_HUD);
    v_present();
}

void Tron_End(void)
{
    char score_text[16];

    if (g_arena_layer) {
        _ffree(g_arena_layer);
        g_arena_layer = NULL;
    }
    g_arena_ready = 0;
    high_scores_format_score(score_text, sizeof(score_text), g_final_score);
    snprintf(g_end_detail, sizeof(g_end_detail), "PUNTOS %s", score_text);
    tron_free_sprites();
}

int Tron_IsFinished(void)
{
    return g_finished;
}

int Tron_DidWin(void)
{
    return g_did_win;
}

const char *Tron_GetEndDetail(void)
{
    return g_end_detail;
}

uint64_t Tron_GetScore(void)
{
    return g_final_score;
}
