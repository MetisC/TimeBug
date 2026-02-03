#ifndef VIDEO_H
#define VIDEO_H

#define VIDEO_WIDTH 320
#define VIDEO_HEIGHT 200

void v_init_mode13(void);
void v_text_mode(void);
void v_clear(unsigned char color);
void v_puts(int x, int y, const char *text, unsigned char color);
void v_present(void);
unsigned char far *v_backbuffer_ptr(void);
void v_present_fast(void);
void v_blit_fullscreen_fast(const unsigned char far *src);
void v_putpixel(int x, int y, unsigned char color);
void v_fill_rect(int x, int y, int w, int h, unsigned char color);
void v_blit_sprite(int x, int y, int w, int h, const unsigned char far *pixels, unsigned char transparent);
void v_set_palette_raw(const unsigned char *rgb, int count);
void v_load_palette(const char *filename);
void v_lock_palette(const char *filename);
void v_draw_dotted_rect(int x, int y, int w, int h, unsigned char base_color, unsigned char dot_color,
                        int horizontal);

#endif
