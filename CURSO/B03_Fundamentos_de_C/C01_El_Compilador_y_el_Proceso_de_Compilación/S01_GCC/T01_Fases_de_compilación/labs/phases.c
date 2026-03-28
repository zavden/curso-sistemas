#include <stdio.h>

#define GREETING "Hello"
#define REPEAT 3

int main(void) {
    for (int i = 0; i < REPEAT; i++) {
        printf("%s, world! (%d)\n", GREETING, i);
    }
    return 0;
}
