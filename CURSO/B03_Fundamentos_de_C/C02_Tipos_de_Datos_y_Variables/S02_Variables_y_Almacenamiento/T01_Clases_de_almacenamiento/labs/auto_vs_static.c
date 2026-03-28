#include <stdio.h>

void counter_auto(void) {
    int count = 0;
    count++;
    printf("  auto:   count = %d\n", count);
}

void counter_static(void) {
    static int count = 0;
    count++;
    printf("  static: count = %d\n", count);
}

int main(void) {
    printf("Call 1:\n");
    counter_auto();
    counter_static();

    printf("Call 2:\n");
    counter_auto();
    counter_static();

    printf("Call 3:\n");
    counter_auto();
    counter_static();

    return 0;
}
