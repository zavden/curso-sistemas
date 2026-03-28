#include <stdio.h>

struct Sensor {
    int id;
    double temperature;
    double humidity;
    int active;
    char name[20];
};

int main(void) {
    /* --- Inicializacion parcial de arrays --- */
    printf("=== Inicializacion parcial de arrays ===\n");

    int partial[8] = {10, 20, 30};

    printf("partial: ");
    for (int i = 0; i < 8; i++) {
        printf("%d ", partial[i]);
    }
    printf("\n");
    printf("Solo los 3 primeros tienen valor, el resto es 0\n");

    /* --- Zero-initialization con {0} --- */
    printf("\n=== Zero-initialization con {0} ===\n");

    int zeros[10] = {0};
    char str_buf[20] = {0};

    printf("zeros: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", zeros[i]);
    }
    printf("\n");

    printf("str_buf como string: \"%s\" (vacio, todo NUL)\n", str_buf);
    printf("str_buf[0] = %d, str_buf[19] = %d\n", str_buf[0], str_buf[19]);

    /* --- Zero-initialization de structs con {0} --- */
    printf("\n=== Zero-initialization de structs con {0} ===\n");

    struct Sensor s_zero = {0};

    printf("s_zero.id:          %d\n", s_zero.id);
    printf("s_zero.temperature: %f\n", s_zero.temperature);
    printf("s_zero.humidity:    %f\n", s_zero.humidity);
    printf("s_zero.active:      %d\n", s_zero.active);
    printf("s_zero.name:        \"%s\"\n", s_zero.name);

    /* --- Inicializacion parcial de structs --- */
    printf("\n=== Inicializacion parcial de structs ===\n");

    struct Sensor s_partial = { .id = 7, .temperature = 22.5 };

    printf("s_partial.id:          %d\n", s_partial.id);
    printf("s_partial.temperature: %f\n", s_partial.temperature);
    printf("s_partial.humidity:    %f (implicito)\n", s_partial.humidity);
    printf("s_partial.active:      %d (implicito)\n", s_partial.active);
    printf("s_partial.name:        \"%s\" (implicito)\n", s_partial.name);

    /* --- Array de structs con inicializacion parcial --- */
    printf("\n=== Array de structs con {0} ===\n");

    struct Sensor sensors[3] = {
        { .id = 1, .temperature = 20.0, .name = "sala" },
        { .id = 2, .name = "cocina" },
        /* el tercer elemento: todo 0 */
    };

    for (int i = 0; i < 3; i++) {
        printf("sensors[%d]: id=%d temp=%.1f humid=%.1f active=%d name=\"%s\"\n",
               i, sensors[i].id, sensors[i].temperature,
               sensors[i].humidity, sensors[i].active, sensors[i].name);
    }

    return 0;
}
