/* search_path.c — Demuestra #include <> vs "" y la opcion -I */

#include <stdio.h>
#include <greeting.h>   /* requiere -I para encontrar greeting.h */

int main(void) {
    printf("Este programa usa un header de un directorio personalizado.\n");
    greet("Preprocesador");
    return 0;
}
