/* preprocess_inspect.c — Para inspeccionar con gcc -E la expansion del preprocesador */

#include <stdio.h>
#include "point.h"

int main(void) {
    struct Point p = { 1, 2 };
    printf("Point(%d, %d)\n", p.x, p.y);
    return 0;
}
