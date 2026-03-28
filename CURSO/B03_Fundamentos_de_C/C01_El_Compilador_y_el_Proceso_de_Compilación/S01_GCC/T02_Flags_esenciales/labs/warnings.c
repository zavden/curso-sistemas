#include <stdio.h>

struct Config {
    int width;
    int height;
    int depth;
};

int process(int x, int unused_param) {
    int y;
    int unused_var = 42;
    unsigned int u = 10;
    int s = -1;

    if (s < u) {
        printf("%s\n", x);
    }

    struct Config cfg = { 640, 480 };

    return y + cfg.width;
}

int main(void) {
    int result = process(5, 0);
    printf("result: %d\n", result);
    return 0;
}
