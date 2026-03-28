#include <stdio.h>

int main(int argc, char **argv) {
    printf("argc = %d\n\n", argc);

    /* Recorrer con indice */
    printf("--- Recorrido con indice ---\n");
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = \"%s\"  (en %p)\n", i, argv[i], (void *)argv[i]);
    }

    /* Verificar el terminador NULL */
    printf("\nargv[argc] = %p (debe ser NULL)\n", (void *)argv[argc]);

    /* Recorrer con puntero a puntero */
    printf("\n--- Recorrido con char **ptr ---\n");
    char **ptr = argv;
    while (*ptr != NULL) {
        printf("*ptr = \"%s\"\n", *ptr);
        ptr++;
    }

    /* Acceso caracter por caracter al primer argumento */
    if (argc > 0) {
        printf("\n--- Caracteres de argv[0] ---\n");
        for (int i = 0; argv[0][i] != '\0'; i++) {
            printf("argv[0][%d] = '%c'\n", i, argv[0][i]);
        }
    }

    return 0;
}
