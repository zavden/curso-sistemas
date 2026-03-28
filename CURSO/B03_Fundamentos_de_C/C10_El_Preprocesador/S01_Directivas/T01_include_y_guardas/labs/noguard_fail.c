/* noguard_fail.c — Demuestra el error de incluir un header sin guardas dos veces */

#include "point_noguard.h"
#include "point_noguard.h"   /* segunda inclusion: error de redefinicion */

#include <stdio.h>

int main(void) {
    struct PointNG p = { 5, 10 };
    printf("PointNG(%d, %d)\n", p.x, p.y);
    return 0;
}
