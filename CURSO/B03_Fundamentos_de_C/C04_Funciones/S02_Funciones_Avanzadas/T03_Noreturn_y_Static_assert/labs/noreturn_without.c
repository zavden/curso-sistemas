#include <stdio.h>
#include <stdlib.h>

/* No noreturn specifier -- just a regular function */
void die(const char *msg) {
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
    printf("Value for type 2: %d\n", get_value(2));
    return 0;
}
