/* pragma_once_demo.c — Usa #pragma once para evitar inclusion doble */

#include "pragma_once_demo.h"
#include "pragma_once_demo.h"   /* segunda inclusion: #pragma once la ignora */
#include <stdio.h>

struct Color color_create(unsigned char r, unsigned char g, unsigned char b) {
    struct Color c = { r, g, b };
    return c;
}

void color_print(const struct Color *c) {
    printf("Color(r=%u, g=%u, b=%u)\n", c->r, c->g, c->b);
}

int main(void) {
    struct Color red = color_create(255, 0, 0);
    printf("Incluido dos veces con #pragma once, sin error.\n");
    color_print(&red);
    return 0;
}
