/* pragma_once_demo.h — Header con #pragma once en vez de include guards */

#pragma once

struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct Color color_create(unsigned char r, unsigned char g, unsigned char b);
void color_print(const struct Color *c);
