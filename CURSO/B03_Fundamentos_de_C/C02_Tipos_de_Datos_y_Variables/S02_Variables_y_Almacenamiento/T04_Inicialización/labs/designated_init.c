#include <stdio.h>

enum Color { RED, GREEN, BLUE, COLOR_COUNT };

struct Config {
    int port;
    int max_connections;
    int timeout_ms;
    int buffer_size;
    int debug_level;
};

int main(void) {
    /* --- Designated initializers en structs --- */
    printf("=== Designated initializers: structs ===\n");

    struct Config cfg = {
        .port = 8080,
        .max_connections = 100,
        .timeout_ms = 5000,
        /* buffer_size y debug_level no mencionados -> 0 */
    };

    printf("port:            %d\n", cfg.port);
    printf("max_connections: %d\n", cfg.max_connections);
    printf("timeout_ms:      %d\n", cfg.timeout_ms);
    printf("buffer_size:     %d (implicito)\n", cfg.buffer_size);
    printf("debug_level:     %d (implicito)\n", cfg.debug_level);

    /* --- Designated initializers en arrays --- */
    printf("\n=== Designated initializers: arrays ===\n");

    int sparse[10] = { [2] = 42, [7] = 99 };

    printf("sparse: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", sparse[i]);
    }
    printf("\n");

    /* --- Array indexado por enum --- */
    printf("\n=== Array indexado por enum ===\n");

    const char *color_name[COLOR_COUNT] = {
        [RED]   = "red",
        [GREEN] = "green",
        [BLUE]  = "blue",
    };

    for (int i = 0; i < COLOR_COUNT; i++) {
        printf("color_name[%d] = \"%s\"\n", i, color_name[i]);
    }

    /* --- Designated initializers: posicional despues de designado --- */
    printf("\n=== Posicional despues de designado ===\n");

    int mixed[6] = { [2] = 10, 20, 30 };

    printf("mixed: ");
    for (int i = 0; i < 6; i++) {
        printf("%d ", mixed[i]);
    }
    printf("\n");
    printf("Despues de [2]=10, los valores 20 y 30 van a indices 3 y 4\n");

    return 0;
}
