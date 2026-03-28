#include <stdio.h>

enum Color { COLOR_RED, COLOR_GREEN, COLOR_BLUE };
enum Fruit { FRUIT_RED, FRUIT_GREEN, FRUIT_YELLOW };

int main(void) {
    printf("--- Enum es int ---\n");
    enum Color c = COLOR_RED;
    int n = c;
    printf("enum Color c = COLOR_RED;\n");
    printf("int n = c;          -> n = %d\n", n);

    c = 42;
    printf("c = 42;             -> c = %d (compila sin error)\n", c);

    c = -1;
    printf("c = -1;             -> c = %d (compila sin error)\n", c);

    printf("\n--- Aritmetica con enums ---\n");
    int sum = COLOR_RED + COLOR_GREEN + COLOR_BLUE;
    printf("COLOR_RED + COLOR_GREEN + COLOR_BLUE = %d\n", sum);

    printf("\n--- Sin type safety ---\n");
    enum Color color = COLOR_RED;
    enum Fruit fruit = FRUIT_RED;
    printf("COLOR_RED = %d, FRUIT_RED = %d\n", color, fruit);
    printf("Son distintos tipos pero ambos valen %d\n", COLOR_RED);

    /* Se pueden mezclar sin error en C: */
    color = FRUIT_GREEN;
    printf("color = FRUIT_GREEN; -> color = %d (compila sin error en C)\n", color);

    printf("\n--- Namespace global ---\n");
    printf("COLOR_RED y FRUIT_RED coexisten porque tienen prefijos distintos.\n");
    printf("Sin prefijo, dos enums con el mismo nombre causarian error.\n");

    return 0;
}
