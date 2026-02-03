#ifndef SPRITE_DAT_H
#define SPRITE_DAT_H

int sprite_dat_load_auto(const char *path, unsigned short *out_w, unsigned short *out_h,
                         unsigned char far *dst, unsigned long max_pixels);

#endif
