#include <stdio.h>

struct Point {
    int x;
    int y;
};

void draw_line(struct Point from, struct Point to) {
    printf("Linea de (%d, %d) a (%d, %d)\n", from.x, from.y, to.x, to.y);
}

struct Point midpoint(struct Point a, struct Point b) {
    return (struct Point){ .x = (a.x + b.x) / 2, .y = (a.y + b.y) / 2 };
}

int sum_array(const int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
    }
    return total;
}

int main(void) {
    /* --- Compound literal como argumento de funcion --- */
    printf("=== Compound literals como argumentos ===\n");

    draw_line(
        (struct Point){ .x = 0, .y = 0 },
        (struct Point){ .x = 10, .y = 20 }
    );

    draw_line(
        (struct Point){ .x = 5, .y = 5 },
        (struct Point){ .x = 15, .y = 25 }
    );

    /* --- Compound literal como retorno de funcion --- */
    printf("\n=== Compound literal como retorno ===\n");

    struct Point m = midpoint(
        (struct Point){ .x = 0, .y = 0 },
        (struct Point){ .x = 10, .y = 20 }
    );
    printf("Midpoint: (%d, %d)\n", m.x, m.y);

    /* --- Compound literal con arrays --- */
    printf("\n=== Compound literal con arrays ===\n");

    int result = sum_array((int[]){10, 20, 30, 40}, 4);
    printf("sum_array({10,20,30,40}) = %d\n", result);

    /* --- Compound literal asignado a puntero --- */
    printf("\n=== Compound literal y punteros ===\n");

    int *ptr = (int[]){100, 200, 300};
    printf("ptr[0]=%d  ptr[1]=%d  ptr[2]=%d\n", ptr[0], ptr[1], ptr[2]);

    return 0;
}
