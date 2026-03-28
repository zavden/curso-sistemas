/* point.c — Implementacion de point.h */

#include "point.h"
#include <stdio.h>

struct Point point_create(int x, int y) {
    struct Point p = { x, y };
    return p;
}

void point_print(const struct Point *p) {
    printf("Point(%d, %d)\n", p->x, p->y);
}
