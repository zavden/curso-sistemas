/* shape.c — Implementacion de shape.h */

#include "shape.h"
#include <stdio.h>

void rectangle_print(const struct Rectangle *r) {
    printf("Rectangle at (%d, %d), %dx%d\n",
           r->origin.x, r->origin.y, r->width, r->height);
}
