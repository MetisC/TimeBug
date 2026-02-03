#include "sprite_dat.h"

#include <stdio.h>

int sprite_dat_load_auto(const char *path, unsigned short *out_w, unsigned short *out_h,
                         unsigned char far *dst, unsigned long max_pixels)
{
    FILE *file;
    long file_size;
    unsigned char header[4];
    size_t read_bytes;
    unsigned char w8;
    unsigned char h8;
    unsigned short w16;
    unsigned short h16;
    unsigned long size_ul;
    long expected;

    if (!path || !out_w || !out_h || !dst || max_pixels == 0) {
        return 0;
    }

    *out_w = 0;
    *out_h = 0;

    file = fopen(path, "rb");
    if (!file) {
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return 0;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    if (file_size >= 4) {
        read_bytes = fread(header, 1, 4, file);
    } else if (file_size >= 2) {
        read_bytes = fread(header, 1, 2, file);
    } else {
        fclose(file);
        return 0;
    }

    if (read_bytes < 2) {
        fclose(file);
        return 0;
    }

    w8 = header[0];
    h8 = header[1];
    size_ul = (unsigned long)w8 * (unsigned long)h8;
    expected = 2L + (long)size_ul;
    if (w8 > 0 && h8 > 0 && file_size == expected && size_ul <= max_pixels) {
        if (fseek(file, 2, SEEK_SET) != 0) {
            fclose(file);
            return 0;
        }
        if ((int)fread(dst, 1, (size_t)size_ul, file) != (int)size_ul) {
            fclose(file);
            return 0;
        }
        *out_w = (unsigned short)w8;
        *out_h = (unsigned short)h8;
        fclose(file);
        return 1;
    }

    if (file_size >= 4 && read_bytes >= 4) {
        w16 = (unsigned short)header[0] | ((unsigned short)header[1] << 8);
        h16 = (unsigned short)header[2] | ((unsigned short)header[3] << 8);
        size_ul = (unsigned long)w16 * (unsigned long)h16;
        expected = 4L + (long)size_ul;
        if (w16 > 0 && h16 > 0 && file_size == expected && size_ul <= max_pixels) {
            if (fseek(file, 4, SEEK_SET) != 0) {
                fclose(file);
                return 0;
            }
            if ((int)fread(dst, 1, (size_t)size_ul, file) != (int)size_ul) {
                fclose(file);
                return 0;
            }
            *out_w = w16;
            *out_h = h16;
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}
