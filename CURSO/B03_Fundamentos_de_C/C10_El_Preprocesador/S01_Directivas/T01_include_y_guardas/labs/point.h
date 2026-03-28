/* point.h — Header CON include guard */

#ifndef POINT_H
#define POINT_H

struct Point {
    int x;
    int y;
};

/* Crea un Point con los valores dados */
struct Point point_create(int x, int y);

/* Imprime un Point a stdout */
void point_print(const struct Point *p);

#endif /* POINT_H */
