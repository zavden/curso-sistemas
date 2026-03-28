/* guards_ok.c — Include guards previenen inclusion doble */

/* Incluye point.h directamente Y a traves de shape.h.
   Sin guardas esto seria un error. Con guardas, funciona. */

#include "point.h"
#include "shape.h"   /* shape.h tambien incluye point.h */
#include <stdio.h>

int main(void) {
    struct Point p = point_create(3, 7);
    printf("Punto creado: ");
    point_print(&p);

    struct Rectangle r = { point_create(1, 2), 10, 5 };
    printf("Rectangulo: ");
    rectangle_print(&r);

    return 0;
}
