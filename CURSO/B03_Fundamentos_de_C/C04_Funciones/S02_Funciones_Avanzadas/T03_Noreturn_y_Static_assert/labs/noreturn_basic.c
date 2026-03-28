#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

noreturn void die(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);
}

int get_value(int type) {
    switch (type) {
        case 1: return 10;
        case 2: return 20;
        case 3: return 30;
    }
    die("unknown type");
}

int main(void) {
    printf("Value for type 1: %d\n", get_value(1));
    printf("Value for type 2: %d\n", get_value(2));
    printf("Value for type 3: %d\n", get_value(3));
    return 0;
}
