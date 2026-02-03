#include "cutscene.h"

#include "../CORE/video.h"
#include "../CORE/input.h"
#include "../CORE/keyboard.h"
#include "../CORE/sprite_dat.h"
#include "../CORE/timer.h"

#include <stdio.h>
#include <string.h>

#define CUTSCENE_TEXTBOX_H 56
#define CUTSCENE_SPRITE_W 96
#define CUTSCENE_SPRITE_H 128
#define CUTSCENE_LINE_HEIGHT 12
#define CUTSCENE_TEXT_MARGIN_X 8
#define CUTSCENE_TEXT_MARGIN_Y 6
#define CUTSCENE_CHARS_PER_SEC 45

static unsigned char g_cutscene_sprite_pixels[CUTSCENE_SPRITE_W * CUTSCENE_SPRITE_H];

static int cutscene_sprite_x(char pos)
{
    if (pos == 'L') {
        return 16;
    }
    if (pos == 'R') {
        return VIDEO_WIDTH - CUTSCENE_SPRITE_W - 16;
    }
    return (VIDEO_WIDTH - CUTSCENE_SPRITE_W) / 2;
}

static void cutscene_decode_text(const char *src, char *dst, size_t dst_size)
{
    size_t i = 0;
    size_t j = 0;

    if (!src || !dst || dst_size == 0) {
        return;
    }

    while (src[i] != '\0' && j + 1 < dst_size) {
        if (src[i] == '\\' && src[i + 1] == 'n') {
            dst[j++] = '\n';
            i += 2;
            continue;
        }
        dst[j++] = src[i++];
    }
    dst[j] = '\0';
}

static void cutscene_draw_text(const char *text, size_t visible_chars)
{
    char visible[512];
    char *line;
    int line_idx = 0;
    int text_y = VIDEO_HEIGHT - CUTSCENE_TEXTBOX_H;
    int y = text_y + CUTSCENE_TEXT_MARGIN_Y;

    if (!text) {
        return;
    }

    if (visible_chars >= sizeof(visible)) {
        visible_chars = sizeof(visible) - 1;
    }

    memcpy(visible, text, visible_chars);
    visible[visible_chars] = '\0';

    v_fill_rect(0, text_y, VIDEO_WIDTH, CUTSCENE_TEXTBOX_H, 0);

    line = visible;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) {
            *next = '\0';
        }

        v_puts(CUTSCENE_TEXT_MARGIN_X, y + (line_idx * CUTSCENE_LINE_HEIGHT), line, 15);
        line_idx++;
        if (!next) {
            break;
        }
        line = next + 1;
    }
}

static void cutscene_wait_for_escape_release(void)
{
    while (kb_down(SC_ESC)) {
        in_poll();
    }
    for (;;) {
        JoystickState state;
        if (!in_joystick_state(&state)) {
            break;
        }
        if (!(state.buttons & JOY_BUTTON_ESC)) {
            break;
        }
        in_poll();
    }
    in_clear();
}

int Cutscene_Play(const char *scene_id)
{
    FILE *file;
    char line[512];
    char current_sprite[64] = "";
    char current_pos = 'C';
    unsigned short sprite_w = 0;
    unsigned short sprite_h = 0;
    int sprite_visible = 0;
    int sprite_x = 0;
    int sprite_y = VIDEO_HEIGHT - CUTSCENE_TEXTBOX_H - CUTSCENE_SPRITE_H;

    if (!scene_id || scene_id[0] == '\0') {
        return 1;
    }

    file = fopen("CUTS.TXT", "r");
    if (!file) {
        v_clear(0);
        v_puts(8, 8, "CUTS.TXT NO ENCONTRADO", 12);
        v_puts(8, 20, "REVISA EL DIRECTORIO ACTUAL", 15);
        v_present();
        while (in_poll() == IN_KEY_NONE) { }
        return 1;
    }

    v_clear(0);
    sprite_visible = 0;
    current_sprite[0] = '\0';

    while (fgets(line, sizeof(line), file)) {
        char *fields[5] = {0};
        char *p = line;
        int field = 0;

        while (*p) {
            if (*p == '\r' || *p == '\n') {
                *p = '\0';
                break;
            }
            p++;
        }

        fields[0] = line;
        for (p = line; *p && field < 4; ++p) {
            if (*p == '|') {
                *p = '\0';
                fields[++field] = p + 1;
            }
        }
        if (field < 4) {
            continue;
        }

        if (strcmp(fields[0], scene_id) != 0) {
            continue;
        }

        {
            const char *sprite_name = fields[1];
            const char *pos_text = fields[2];
            const char *clear_text = fields[3];
            const char *raw_text = fields[4];
            char text[512];
            char pos = 'C';
            int clear_screen = 0;
            int needs_sprite = 0;
            int sprite_changed = 0;
            size_t text_len = 0;
            size_t visible = 0;
            uint32_t start_us = timer_now_us();

            if (pos_text && pos_text[0]) {
                pos = pos_text[0];
            }
            clear_screen = (clear_text && clear_text[0] == '1');
            cutscene_decode_text(raw_text, text, sizeof(text));
            text_len = strlen(text);

            if (clear_screen) {
                v_clear(0);
                sprite_visible = 0;
                current_sprite[0] = '\0';
            }

            if (sprite_name && sprite_name[0] != '\0' && sprite_name[0] != '-') {
                needs_sprite = 1;
            }

            if (needs_sprite) {
                if (!sprite_visible || strcmp(current_sprite, sprite_name) != 0 || current_pos != pos) {
                    sprite_changed = 1;
                }
            } else if (sprite_visible) {
                sprite_changed = 1;
            }

            if (sprite_changed && sprite_visible) {
                v_fill_rect(sprite_x, sprite_y, sprite_w ? sprite_w : CUTSCENE_SPRITE_W,
                            sprite_h ? sprite_h : CUTSCENE_SPRITE_H, 0);
            }

            if (needs_sprite) {
                if (sprite_changed) {
                    char path[96];
                    snprintf(path, sizeof(path), "SPRITES\\%s.dat", sprite_name);
                    if (sprite_dat_load_auto(path, &sprite_w, &sprite_h,
                                             (unsigned char far *)g_cutscene_sprite_pixels,
                                             (unsigned long)sizeof(g_cutscene_sprite_pixels))) {
                        sprite_x = cutscene_sprite_x(pos);
                        v_blit_sprite(sprite_x, sprite_y, sprite_w, sprite_h,
                                      (const unsigned char far *)g_cutscene_sprite_pixels, 0);
                        sprite_visible = 1;
                        current_pos = pos;
                        strncpy(current_sprite, sprite_name, sizeof(current_sprite) - 1);
                        current_sprite[sizeof(current_sprite) - 1] = '\0';
                    } else {
                        sprite_visible = 0;
                        current_sprite[0] = '\0';
                    }
                }
            } else {
                sprite_visible = 0;
                current_sprite[0] = '\0';
            }

            in_clear();

            for (;;) {
                int key = in_poll();
                uint32_t now_us = timer_now_us();
                size_t target = (size_t)(((uint64_t)(now_us - start_us) * CUTSCENE_CHARS_PER_SEC) / 1000000ULL);

                if (target > text_len) {
                    target = text_len;
                }
                if (visible < target) {
                    visible = target;
                }

                if (key == IN_KEY_ESC) {
                    cutscene_wait_for_escape_release();
                    fclose(file);
                    return 0;
                }
                if (key == IN_KEY_SPACE || key == IN_KEY_ENTER) {
                    if (visible < text_len) {
                        visible = text_len;
                    } else {
                        break;
                    }
                }

                cutscene_draw_text(text, visible);
                v_present();
            }
        }
    }

    fclose(file);
    return 1;
}
