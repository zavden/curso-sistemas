/* shape.h — Incluye point.h (genera inclusion transitiva) */

#ifndef SHAPE_H
#define SHAPE_H

#include "point.h"

struct Rectangle {
    struct Point origin;
    int width;
    int height;
};

void rectangle_print(const struct Rectangle *r);

#endif /* SHAPE_H */
